//
// Created by Alun on 5/11/2015.
//

#include "webmesh.h"
#include <png++/png.hpp>
#include <sys/types.h>
#include <sys/stat.h>
#include "tipsify.cpp"
#include "forsyth.cpp"
#include "tiny_obj_loader.h"
#include <string.h>
#include <stdlib.h>
#include "filewriter.h"

#define EPSILON 0.0001

WebMesh::WebMesh(string infile, string out_root) {
    m_INFILE = infile;
    m_OUT_ROOT = out_root;
    m_OUT_FILENAME = out_root.substr(out_root.find_last_of('/')+1, out_root.length());
    printf("%s", m_OUT_FILENAME.c_str());
    m_bV = 12;
    m_bN = 12;
    m_bT = 12;
    m_meshEncoding = MeshEncoding::VARINT;
    m_indCoding = IndexCoding::HIGHWATER;
    m_indCompression = IndexCompression::PAIRED_TRIS;
    m_triOrdering = TriangleReordering::FORSYTH;
    m_vertexQuantization = VertexQuantization::GLOBAL;
    m_normalEncoding = NormalEncoding::OCT;
    m_fibLevels = 1;
}
void WebMesh::setQuantizationBitDepth(int aBV, int aBN, int aBT){m_bV = aBV;m_bN = aBN;m_bT = aBT;}
void WebMesh::setQuantizationBitDepthPosition(int aBV) {m_bV = aBV;}
void WebMesh::setQuantizationBitDepthNormal(int aBN) {m_bN = aBN;}
void WebMesh::setQuantizationBitDepthTexture(int aBT) {m_bT = aBT;}
void WebMesh::setFibLevels(int aFibLevels){m_fibLevels = aFibLevels;}
void WebMesh::setOutputFormat(MeshEncoding output){ m_meshEncoding = output;}
void WebMesh::setIndexCoding(IndexCoding ic){ m_indCoding = ic;}
void WebMesh::setIndexCompression(IndexCompression ic){ m_indCompression = ic;}
void WebMesh::setTriangleReordering(TriangleReordering t){m_triOrdering = t;}
void WebMesh::setVertexQuantization(VertexQuantization v){m_vertexQuantization = v;}
void WebMesh::setNormalEncoding(NormalEncoding n){m_normalEncoding = n;}

int pushSafeInterleavedUTF(int delta, vector<int>& indices) {
    int interleaved = Geo::getInterleavedUint(delta);
    if (interleaved >= 55295 && interleaved <= 57343) {
        if (delta < 0)
            delta = delta + 14000;
        else
            delta = delta - 14000;
        indices.push_back(55295);
        indices.push_back(Geo::getInterleavedUint(delta));
        return 0; // we know that the stored value is less than 55295 to not four bytes
    }
    else {
        indices.push_back(interleaved);
        if (interleaved > 65535) return 1; // its a fourbyter
        else return 0;
    }

}

void pushSafeUTF(int value, vector<int>& indices) {
    if (value >= 55295 && value <= 57343) {
        value -= 14000;
        indices.push_back(55295);
        indices.push_back(value);
    }
    else
        indices.push_back(value);
}

//returns 1 if there are no normals
int readShapeMesh(tinyobj::mesh_t m, vector<Vertex*>& ORIGVERTICES, vector<int>& ORIGINDICES) {

    int toReturn = 0;
    if (m.normals.size() == 0)
        toReturn = 1;

    int zeroNormals = 0;
    for (int i = 0; i < m.positions.size()/3; i++) {

        Vertex *newVertex = new Vertex(vec3(m.positions[i*3],m.positions[i*3+1],m.positions[i*3+2]));
        vec3 n;
        if (m.normals.size() == m.positions.size()) {
            n = vec3(m.normals[i*3],m.normals[i*3+1],m.normals[i*3+2]);
            if (abs(n.x) == 0.0 && abs(n.y) == 0.0 && abs(n.z) == 0.0 ) {
                n = vec3(0.0001, 0.0001, 0.0001);
                zeroNormals++;
            }
        }
        else {
            n = vec3(0.0001, 0.0001, 0.0001);
            zeroNormals++;
        }
        newVertex->normal = glm::normalize(n);

        if (m.texcoords.size() != 0)
            newVertex->texture = vec2(m.texcoords[i*2],m.texcoords[i*2+1]);
        ORIGVERTICES.push_back(newVertex);
    }
    //printf("Replaced %d zeroed Normals\n", zeroNormals);
    for (int i = 0; i < m.indices.size(); i++)
        ORIGINDICES.push_back((int)m.indices[i]);

    return toReturn;
}

void generateQuantizedVertices(vector<Vertex*>& ORIGVERTICES,
                               AABB* aabb, int m_bV, int m_bN, int m_bT,
                               vector<Vertex*>& VERTICES,
                                VertexQuantization m_vertexQuantization,
                                vector<int>& out_m_bV,
                                int m_fibLevels) {

    map<Vertex, int> vertex_int_map;
    map<int, int> int_int_map; //old->new
    std::map<Vertex, int>::iterator it;

    int m_bVx, m_bVy, m_bVz;
    m_bVx = m_bVy = m_bVz = 0;
    // if we quantize per axis, calculate if can save
    if (m_vertexQuantization == VertexQuantization::PER_AXIS) {
        //find longest axis; 0 = x, 1 = y, 2 = z
        int longAxis = 0;
        if (aabb->range.y > aabb->range.x && aabb->range.y > aabb->range.z )
            longAxis = 1;
        else if (aabb->range.z > aabb->range.x && aabb->range.z > aabb->range.y)
            longAxis = 2;

        if (longAxis == 0) {
            m_bVx = m_bV;
            int intFactorY = (int) (aabb->range.x / aabb->range.y);
            m_bVy = m_bV - (intFactorY - 1);
            int intFactorZ = (int) (aabb->range.x / aabb->range.z);
            m_bVz = m_bV - (intFactorZ - 1);
        }
        if (longAxis == 1) {
            int intFactorX = (int) (aabb->range.y / aabb->range.x);
            m_bVx = m_bV - (intFactorX - 1);
            m_bVy = m_bV;
            int intFactorZ = (int) (aabb->range.y / aabb->range.z);
            m_bVz = m_bV - (intFactorZ - 1);
        }
        if (longAxis == 2) {

            int intFactorX = (int) (aabb->range.z / aabb->range.x);
            m_bVx = m_bV - (intFactorX - 1);
            int intFactorY = (int) (aabb->range.z / aabb->range.y);
            m_bVy = m_bV - (intFactorY - 1);
            m_bVz = m_bV;
        }

        printf("long axis %d\n", longAxis);
        printf("m_bVx %d, m_bVy %d, m_bVz %d\n", m_bVx, m_bVy, m_bVz);
        out_m_bV.push_back(m_bVx);
        out_m_bV.push_back(m_bVy);
        out_m_bV.push_back(m_bVz);
    } else {
        out_m_bV.push_back(m_bV);
        out_m_bV.push_back(m_bV);
        out_m_bV.push_back(m_bV);
    }


    vector<vec3> fibSphere = Geo::computeFibonacci_sphere(m_fibLevels);

    // std::map<vector<int>, int> normalsMap;
    // std::map<vector<int>, int>::iterator itt;
    // int normalsMapCounter = 0;

    //quantize everything
    for (int i = 0; i < ORIGVERTICES.size(); i++){
        Vertex * currVert = ORIGVERTICES[i];
        vector<int> currPos;
        if (m_vertexQuantization == VertexQuantization::GLOBAL)
            currPos = Geo::quantizeVertexPosition(currVert->pos, aabb, m_bV);
        else if (m_vertexQuantization == VertexQuantization::PER_AXIS)
            currPos = Geo::quantizeVertexPosition(currVert->pos, aabb, m_bVx, m_bVy, m_bVz);
        else { printf("Unknown vertex quantization method"); exit(0);}

        vector<int> currNorm = Geo::quantizeVertexNormal(currVert->normal, m_bN);

        Vertex *newVertex = new Vertex(vec3(currPos[0], currPos[1], currPos[2]));
        newVertex->normal = vec3(currNorm[0],currNorm[1],currNorm[2]);

        //find normals
		// itt = normalsMap.find(currNorm);
		// if (itt == normalsMap.end()) {
  //       	normalsMap[currNorm] = normalsMapCounter++;
  //       }

        newVertex->normal2 = Geo::octEncode_8bit(currVert->normal); // 0- 255
        newVertex->normal1 = Geo::computeFibonacci_normal(currVert->normal, fibSphere);

        vector<int> currCoords = Geo::quantizeVertexTexture(currVert->texture, m_bT);
        newVertex->texture = vec2(currCoords[0],currCoords[1]);

        VERTICES.push_back(newVertex);
    }
    // printf("num verts: %lu; numNormals %d", VERTICES.size(), normalsMapCounter);
}

void removeDuplicateVertices(vector<Vertex*>& in_vertices,
                             vector<Vertex*>& out_vertices,
                             vector<int>& in_indices,
                             vector<int>& out_indices) {

    int newVertCounter = 0;
    map<Vertex, int> vertex_int_map;
    std::map<Vertex, int>::iterator it;
    map<int, int> int_int_map; //old->new

    for (size_t i = 0; i < in_vertices.size(); i++) {
        //get pointer to vertex
        Vertex *v = in_vertices[i];
        //dereference pointer to get vertex struct
        Vertex nv = *v;
        //search for struct in map
        it = vertex_int_map.find(nv);
        //if we don't find it
        if (it == vertex_int_map.end()) {
            //add to out_verts
            out_vertices.push_back(v);
            //add to map for searching
            vertex_int_map[nv] = newVertCounter;
            //add to remapping map
            int_int_map[i] = newVertCounter++;
        }
        else { //we found it
            int_int_map[i] = it->second;
        }
    }

    //printf("pre quantize verts: %d ; post %d\n", (int)in_vertices.size(), (int)out_vertices.size());

    for (size_t i = 0; i < in_indices.size(); i+=3) {
        int A = int_int_map[in_indices[i]];
        int B = int_int_map[in_indices[i+1]];
        int C = int_int_map[in_indices[i+2]];

        if (A == B || B == C || C == A) {
            int bob = 0; // decomposed tri do nothing!
        }
        else { //all inds are different, add tri
            out_indices.push_back(A);
            out_indices.push_back(B);
            out_indices.push_back(C);
        }

    }

    //printf("pre quantize inds: %d ; post %d\n", (int)in_indices.size(), (int)out_indices.size());

}

void reIndexAccordingToIndices(vector<Vertex*>& quantizedVertices,
                               vector<int>& origIndices,
                               map<Vertex, int>& vertex_int_map,
                               vector<Vertex*>& FINAL_VERTICES,
                               vector<int>& indices_out) {
    //reindex
    int newVertCounter = 0;

    std::map<Vertex, int>::iterator it;

    //for each set of three original indices
    for (int i = 0; i < origIndices.size(); i+=3) {
        //creat empty vector for new face indices
        vector<int> newFaceIndices;
        //loop through next three indices
        for (int j = 0; j < 3; j++) {
            //get current index
            int index = origIndices[i+j];
            //get vert referenced by pointer
            Vertex vert = *quantizedVertices[index];

            //try to find vertex in map
            it = vertex_int_map.find(vert);
            //if its there, add the index to the newFaceindex list
            if (it != vertex_int_map.end()) {

                newFaceIndices.push_back(it->second);
            }
                //otherwise, add size-1 of current map as index
                //and add vert to map
            else {
                newFaceIndices.push_back(newVertCounter);
                vertex_int_map[vert] = newVertCounter;
                newVertCounter++; // now increment
            }
        }


        indices_out.push_back(newFaceIndices[0]);
        indices_out.push_back(newFaceIndices[1]);
        indices_out.push_back(newFaceIndices[2]);



    }

    map<int, Vertex*> INT_VERTEX_MAP;
    //now flip the map
    for (auto itr = vertex_int_map.begin(); itr != vertex_int_map.end(); itr++) {
        INT_VERTEX_MAP[itr->second] = (Vertex*)&(itr->first);
    }

    for (int i= 0; i < INT_VERTEX_MAP.size(); i++){
        FINAL_VERTICES.push_back(INT_VERTEX_MAP[i]);
    }

    //now remove duplicate tris from index?

}



int encodeIndicesDelta(vector<int>& FINAL_INDICES, vector<int>& indices_to_write) {

    float al = 0;
    float un = 0;
    int index = 0;
    int fourByters = 0;
    int previousIndex    = 0;
    int toStore = 0;
    for (int i = 0; i < FINAL_INDICES.size(); i++) {

        index = FINAL_INDICES[i];

        toStore = index-previousIndex;
        previousIndex = index;
        al+=abs(toStore);
        un++;

        fourByters += pushSafeInterleavedUTF(toStore, indices_to_write);
    }

    //cout << "num fourbyters: " << fourByters << endl;

    cout << "average store: " << (al/un) << endl;
    return fourByters;
}

int encodeIndicesHighWater(vector<int>& FINAL_INDICES, vector<int>& indices_to_write, int max_step, bool utfSafe) {
    //hwm encode indices
    float al = 0;
    float un = 0;
    int fourByters = 0;

    int toStore = 0; int hi = max_step-1;

    int maxStore = 0;
    for (int v : FINAL_INDICES) {

        assert(v <= hi);

        toStore = (hi - v);
        al += toStore;
        un++;

        if (utfSafe == true) { // add hack to overcome the limitations of utf
            if (toStore > 65535) //count the number of indices which need more than 16-bit to store
                fourByters++;

            pushSafeUTF(toStore, indices_to_write);
        }
        else
            indices_to_write.push_back(toStore); // if we aren't using utf, just push the high water mark

        hi = std::max(hi, v + max_step);

        if (toStore > maxStore)
            maxStore = toStore;


    }

    // cout << "max store: " << maxStore << endl;
    // cout << "num fourbyters: " << fourByters << endl;
    cout << "average store: " << (al/un) << endl;

    return fourByters;
}

void deltaEncodeVertices(vector<Vertex*>& FINAL_VERTICES, vector<int>& VTXDelta, int m_bT,
                        NormalEncoding m_normalEncoding) {
    int lastPosX = 0, lastPosY = 0, lastPosZ = 0,
            lastNormX = 0, lastNormY = 0, lastNormZ = 0,
            lastCoordU = 0, lastCoordV = 0, delta = 0;

    //three channels for positions
    for (int i = 0; i < FINAL_VERTICES.size(); i++) {
        delta = (int)(FINAL_VERTICES[i]->pos.x - lastPosX);
        lastPosX = (int)(FINAL_VERTICES[i]->pos.x);
        VTXDelta.push_back(Geo::getInterleavedUint(delta));
    }
    for (int i = 0; i < FINAL_VERTICES.size(); i++) {
        delta = (int)(FINAL_VERTICES[i]->pos.y - lastPosY);
        lastPosY = (int)(FINAL_VERTICES[i]->pos.y);
        VTXDelta.push_back(Geo::getInterleavedUint(delta));
    }
    for (int i = 0; i < FINAL_VERTICES.size(); i++) {
        delta = (int)(FINAL_VERTICES[i]->pos.z - lastPosZ);
        lastPosZ = (int)(FINAL_VERTICES[i]->pos.z);
        VTXDelta.push_back(Geo::getInterleavedUint(delta));
    }

    //choice of channels for normals
    if (m_normalEncoding == NormalEncoding::FIB) {
        for (int i = 0; i < FINAL_VERTICES.size(); i++) {
            delta = FINAL_VERTICES[i]->normal1 - lastNormX;
            lastNormX = FINAL_VERTICES[i]->normal1;
            VTXDelta.push_back(Geo::getInterleavedUint(delta));
        }
    }
    else if (m_normalEncoding == NormalEncoding::OCT) {
        for (int i = 0; i < FINAL_VERTICES.size(); i++) {
            delta = (int) (FINAL_VERTICES[i]->normal2.x - lastNormX);
            lastNormX = (int) (FINAL_VERTICES[i]->normal2.x);
            VTXDelta.push_back(Geo::getInterleavedUint(delta));
        }
        for (int i = 0; i < FINAL_VERTICES.size(); i++) {
            delta = (int) (FINAL_VERTICES[i]->normal2.y - lastNormY);
            lastNormY = (int) (FINAL_VERTICES[i]->normal2.y);
            VTXDelta.push_back(Geo::getInterleavedUint(delta));
        }
    } else if (m_normalEncoding == NormalEncoding::QUANT) {
        for (int i = 0; i < FINAL_VERTICES.size(); i++) {

            delta = (int) (FINAL_VERTICES[i]->normal.x - lastNormX);
            lastNormX = (int) (FINAL_VERTICES[i]->normal.x);
            VTXDelta.push_back(Geo::getInterleavedUint(delta));

        }
        for (int i = 0; i < FINAL_VERTICES.size(); i++) {
            delta = (int) (FINAL_VERTICES[i]->normal.y - lastNormY);
            lastNormY = (int) (FINAL_VERTICES[i]->normal.y);
            VTXDelta.push_back(Geo::getInterleavedUint(delta));
        }
        for (int i = 0; i < FINAL_VERTICES.size(); i++) {

            delta = (int) (FINAL_VERTICES[i]->normal.z - lastNormZ);
            lastNormZ = (int) (FINAL_VERTICES[i]->normal.z);
            VTXDelta.push_back(Geo::getInterleavedUint(delta));
        }
    }

    //only encode textures if necessary
    if (m_bT > 0) {
        for (int i = 0; i < FINAL_VERTICES.size(); i++) {
            delta = (int)(FINAL_VERTICES[i]->texture.x - lastCoordU);
            lastCoordU = (int)(FINAL_VERTICES[i]->texture.x);
            VTXDelta.push_back(Geo::getInterleavedUint(delta));
        }
        for (int i = 0; i < FINAL_VERTICES.size(); i++) {
            delta = (int)(FINAL_VERTICES[i]->texture.y - lastCoordV);
            lastCoordV = (int)(FINAL_VERTICES[i]->texture.y);
            VTXDelta.push_back(Geo::getInterleavedUint(delta));
        }
    }
}

json WebMesh::writeHeader(vector<int>& vertices_to_write,
		                 vector<int>& indices_to_write,
		                 AABB* aabb,
		                 int numVerts,
		                 int numFaces,
		                 int fourByters,
		                 int max_hw_step,
                         int mat_id,
                         const char* name,
                         int needReconstructNormals,
		                 int m_bVx,
		                 int m_bVy,
		                 int m_bVz) 
{
    json metadata;

    metadata["vertices"] = numVerts;
    metadata["faces"] = numFaces;
    metadata["indexBufferSize"] = indices_to_write.size();
    metadata["utfIndexBuffer"] = int(indices_to_write.size())+fourByters; //add number of 3 or 4 byte indices for utf parsing

    if (m_normalEncoding == NormalEncoding::QUANT)
        metadata["normalEncoding"] = "quantisation";
    else if (m_normalEncoding == NormalEncoding::OCT)
        metadata["normalEncoding"] = "octahedral";
    else if (m_normalEncoding == NormalEncoding::FIB){
        metadata["normalEncoding"] = "fibonacci";
        metadata["fibonacciLevel"] = m_fibLevels;
    }


    json currBits;
    if (m_vertexQuantization == VertexQuantization::GLOBAL) {
        metadata["vertexQuantization"] = "global";
        currBits["vertex"] = m_bV;
    }
    else {
        metadata["vertexQuantization"] = "perVertex";
        json currBitsVTX;
        currBitsVTX["x"] = m_bVx;
        currBitsVTX["y"] = m_bVy;
        currBitsVTX["z"] = m_bVz;
        currBits["vertex"] = currBitsVTX;
    }
    currBits["normal"] = m_bN;
    currBits["texture"] = m_bT;
    metadata["bits"] = move(currBits);

    json headerAABB;
    json headerAABBMin(json::an_array);
    headerAABBMin.add((double)aabb->min[0]);
    headerAABBMin.add((double)aabb->min[1]);
    headerAABBMin.add((double)aabb->min[2]);
    json headerAABBRange(json::an_array);
    headerAABBRange.add((double)aabb->range[0]);
    headerAABBRange.add((double)aabb->range[1]);
    headerAABBRange.add((double)aabb->range[2]);
    headerAABB["range"] = move(headerAABBRange);
    headerAABB["min"] = move(headerAABBMin);
    metadata["AABB"] = move(headerAABB);

    if (m_indCompression == IndexCompression::PAIRED_TRIS)
        metadata["indexCompression"] = "pairedtris";
    else
        metadata["indexCompression"] = "none";

    if (m_meshEncoding == MeshEncoding::UTF) {
        string outUTF8Name = m_OUT_FILENAME + ".utf8";
        struct stat filestatus;
        stat(outUTF8Name.c_str(), &filestatus);
        metadata["data"] = outUTF8Name;
        if (m_indCoding == IndexCoding::DELTA){
            metadata["indexCoding"] = "delta";
        } else {
            metadata["indexCoding"] = "highwater";
            metadata["max_step"] = max_hw_step;
        }

        metadata["size"] = int(filestatus.st_size) / 1000;
    }
    else if (m_meshEncoding == MeshEncoding::PNG) {
        string outPNGName = m_OUT_FILENAME + ".png";
        struct stat filestatus;
        stat(outPNGName.c_str(), &filestatus);
        metadata["data"] = outPNGName;
        if (m_indCoding == IndexCoding::DELTA){
            metadata["indexCoding"] = "delta";
        } else {
            metadata["indexCoding"] = "highwater";
            metadata["max_step"] = max_hw_step;
        }
        metadata["size"] = int(filestatus.st_size) / 1000;
    }
    else if (m_meshEncoding == MeshEncoding::VARINT) {
        string outVARINTName = m_OUT_FILENAME + ".b128";
        struct stat filestatus;
        stat(outVARINTName.c_str(), &filestatus);
        metadata["data"] = outVARINTName;
        if (m_indCoding == IndexCoding::DELTA){
            metadata["indexCoding"] = "delta";
        } else {
            metadata["indexCoding"] = "highwater";
            metadata["max_step"] = max_hw_step;
        }
        metadata["size"] = int(filestatus.st_size) / 1000;

    }

    metadata["mat_id"] = mat_id;

    metadata["name"] = name;

    if (needReconstructNormals == 1) {
        metadata["reconstructNormals"] = true;
    }

	return metadata;
}

void WebMesh::readOBJ(const string m_INFILEPATH) {

    cout << "Reading files..." << endl;


    //load obj file
    //check user hasn't specified a full path
    //if so we need to specify material path
    std::string err; bool ret;
    int slash = m_INFILEPATH.find_last_of('/');
    if (slash == -1){
        ret = tinyobj::LoadObj(m_shapes, m_materials, err, m_INFILEPATH.c_str());
    }
    else {

        std::string prefix = m_INFILEPATH.substr(0, slash+1);
        ret = tinyobj::LoadObj(m_shapes, m_materials, err, m_INFILEPATH.c_str(), prefix.c_str());
        
    }

    if (!err.empty()) { // `err` may contain warning message.
      std::cerr << err << std::endl;
    }
    if (!ret) {
      exit(1);
    }

    cout << "number of separate meshes: " << m_shapes.size() << endl;
}

json WebMesh::processOneShape(tinyobj::shape_t currShape, 
                        vector<int>& vertices_to_write, 
                        vector<int>& indices_to_write) {

    vector<Vertex*> origVertices;
    vector<int> origIndices;
    int needReconstructNormals = readShapeMesh(currShape.mesh, origVertices, origIndices);


    //STEP 2 (optional): reorder indices,
    vector<int> reorder_indices;
    if (m_triOrdering == TriangleReordering::TIPSIFY) {
        int* tip = (int*)tipsify(&origIndices[0], origIndices.size()/3, origVertices.size(), 120);
        reorder_indices.assign(tip, tip+origIndices.size());
    }
    else if (m_triOrdering == TriangleReordering::FORSYTH) {
        int *forth = (int *) reorderForsyth(&origIndices[0], origIndices.size() / 3, origVertices.size());
        reorder_indices.assign(forth, forth + origIndices.size());
    }
    else
        reorder_indices = origIndices;


    //get AABB
    AABB* aabb = Geo::getAABB(origVertices);


    //STEP 4: Quantize vertices - option to encode per axis or straight
    vector<Vertex*> quantizedVertices;
    vector<int> out_m_bV;
    generateQuantizedVertices(origVertices, aabb, m_bV, m_bN, m_bT,
                              quantizedVertices, m_vertexQuantization,
                              out_m_bV, m_fibLevels);

    //STEP 4a: Remove duplicate verts and triangles from quantization process
    vector<Vertex*> dupVerts;
    vector<int> dupInds;
    removeDuplicateVertices(quantizedVertices,
                            dupVerts,
                            reorder_indices,
                            dupInds);

        //STEP 5: Reindex mesh for index compression
    map<Vertex, int> vertex_int_map;
    vector<Vertex*> FINAL_VERTICES;
    vector<int> reIndexed_inds;

    cout << "Reindexing..." << endl;
    //reIndexAccordingToIndices(quantizedVertices, reorder_indices, vertex_int_map, FINAL_VERTICES, reIndexed_inds);
    reIndexAccordingToIndices(dupVerts, dupInds, vertex_int_map, FINAL_VERTICES, reIndexed_inds);



    //we store the final number of faces here, before we compress index buffer
    int finalNumFaces = dupInds.size()/3;

    //STEP 6 (optional): COMPRESS INDICES
    vector<int> FINAL_INDICES;
    int max_step = 1;
    if (m_indCompression == IndexCompression::PAIRED_TRIS) {
        cout << "Compressing indices..." << endl;
        Geo::compressIndexBuffer(reIndexed_inds, FINAL_INDICES);
        max_step = 3;
    }
    else
        FINAL_INDICES = reIndexed_inds;



    //STEP 6: Encode everythings, either delta or HWM 
    cout << "Encoding..." << endl;

    deltaEncodeVertices(FINAL_VERTICES, vertices_to_write, m_bT, m_normalEncoding);

    int fourByters = 0;

    if (m_indCoding == IndexCoding::DELTA){
        fourByters = encodeIndicesDelta(FINAL_INDICES, indices_to_write);
    }
    else {

        if (m_meshEncoding == MeshEncoding::UTF)
            fourByters = encodeIndicesHighWater(FINAL_INDICES, indices_to_write, max_step, true); //true says its UTF safe
        else
            encodeIndicesHighWater(FINAL_INDICES, indices_to_write, max_step, false); //true says its UTF safe
    }

    //write metadata
    json metadata = writeHeader(vertices_to_write,
            indices_to_write,
            aabb,
            int(FINAL_VERTICES.size()),
            finalNumFaces,
            fourByters,
            max_step,
            currShape.mesh.material_ids[0],
            currShape.name.c_str(),
            needReconstructNormals,
            out_m_bV[0], out_m_bV[1], out_m_bV[2]);
    return metadata;

}

json cArrayToJson(float c_arr[], int numElements) {
    json newArr(json::an_array);
    for (int i = 0; i < numElements; i++)
        newArr.add((double)c_arr[i]);
    return newArr;
}

json processOneMaterial(tinyobj::material_t mat, int id) {
    json newMat;
    newMat["name"] = mat.name;
    newMat["id"] = (int)id;
    newMat["Ka"] = move(cArrayToJson(mat.ambient, 3));
    newMat["Kd"] = move(cArrayToJson(mat.diffuse, 3));
    newMat["Ks"] = move(cArrayToJson(mat.specular, 3));
    newMat["Tr"] = move(cArrayToJson(mat.transmittance, 3));
    newMat["Ke"] = move(cArrayToJson(mat.emission, 3));
    newMat["Ns"] = (double)mat.shininess;
    newMat["Ni"] = (double)mat.ior;
    newMat["dissolve"] = (double)mat.dissolve;
    newMat["illum"] = (int)mat.illum;
    newMat["map_Ka"] = mat.ambient_texname.c_str();
    newMat["map_Kd"] = mat.diffuse_texname.c_str();
    newMat["map_Ks"] = mat.specular_texname.c_str();
    newMat["map_Ns"] = mat.specular_highlight_texname.c_str();
    newMat["map_bump"] = mat.bump_texname.c_str();
    newMat["map_d"] = mat.alpha_texname.c_str();
    return newMat;
}

json WebMesh::exportMesh(bool writeHFile) {
	m_writeHeaderFile = writeHFile;


    //STEP 1: read OBJ file - this fills m_shapes and m_materials
    readOBJ(m_INFILE);

    vector<int> buffer_to_write;

    json metadata;
    json metaMeshes(json::an_array);
    json metaMaterials(json::an_array);

    //meshes
    for (int i = 0; i < m_shapes.size(); i++) {
        vector<int> vertices_one;
        vector<int> indices_one;
        json oneMesh = processOneShape(m_shapes[i], vertices_one, indices_one);

        buffer_to_write.insert(buffer_to_write.end(), vertices_one.begin(), vertices_one.end());
        buffer_to_write.insert(buffer_to_write.end(), indices_one.begin(), indices_one.end());

        metaMeshes.add(oneMesh);
    }
    
    //materials
    for (int i = 0; i < m_materials.size(); i++) {
        json oneMat = processOneMaterial(m_materials[i], i);
        metaMaterials.add(oneMat);
    }

    metadata["meshes"] = move(metaMeshes);
    metadata["materials"] = move(metaMaterials);



    if (m_writeHeaderFile) {
        ofstream jsonFile;
        jsonFile.open (m_OUT_ROOT + ".json");
        jsonFile << metadata;
        jsonFile.close();
    }
    
    if (m_meshEncoding == MeshEncoding::UTF)
        //filewriter::writeDataUTF(buffer_to_write, m_OUT_ROOT);
        printf("Sorry UTF writing is currently broken!!");
    else if (m_meshEncoding == MeshEncoding::PNG)
        filewriter::writeDataPNG(buffer_to_write, m_OUT_ROOT);
    else if (m_meshEncoding == MeshEncoding::VARINT)
        filewriter::writeDataVARINT(buffer_to_write, m_OUT_ROOT);

    return metadata;
}




