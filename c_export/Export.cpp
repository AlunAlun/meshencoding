//
// Created by Alun on 16/07/2015.
//

#include "Export.h"
#include <png++/png.hpp>
#include <sys/types.h>
#include <sys/stat.h>
#include "tipsify.cpp"
#include "forsyth.cpp"
#include "tiny_obj_loader.h"

#define EPSILON 0.0001


namespace Export {

    void write_utf8(unsigned code_point, FILE * utfFile)
    {
        if (code_point < 0x80) {
            fputc(code_point, utfFile);
        } else if (code_point <= 0x7FF) {
            fputc((code_point >> 6) + 0xC0, utfFile);
            fputc((code_point & 0x3F) + 0x80, utfFile);
        } else if (code_point <= 0xFFFF) {
            fputc((code_point >> 12) + 0xE0, utfFile);
            fputc(((code_point >> 6) & 0x3F) + 0x80, utfFile);
            fputc((code_point & 0x3F) + 0x80, utfFile);
        } else if (code_point <= 0x10FFFF) {
            fputc((code_point >> 18) + 0xF0, utfFile);
            fputc(((code_point >> 12) & 0x3F) + 0x80, utfFile);
            fputc(((code_point >> 6) & 0x3F) + 0x80, utfFile);
            fputc((code_point & 0x3F) + 0x80, utfFile);
        } else {
            int bob = 0;
            //cout << "invalid code_point " << code_point << endl;
        }
    }

    int pushSafeInterleavedUTF(int delta, vector<u_int_32>& indices) {
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

    void pushSafeUTF(int value, vector<u_int_32>& indices) {
        if (value >= 55295 && value <= 57343) {
            value -= 14000;
            indices.push_back(55295);
            indices.push_back(value);
        }
        else
            indices.push_back(value);
    }

    void readOBJ(const string INFILEPATH, vector<Vertex*>& ORIGVERTICES, vector<int>& ORIGINDICES) {

        cout << "Reading files..." << endl;

        //load obj file
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string err = tinyobj::LoadObj(shapes, materials, INFILEPATH.c_str()); //TODO multiple materials
        if (!err.empty()) {
            std::cerr << err << std::endl;
            exit(1);
        }


        //parse data into Vertex and Face lists

        tinyobj::mesh_t m = shapes[0].mesh;
        int zeroNormals = 0;
        for (int i = 0; i < m.positions.size()/3; i++) {
            Vertex *newVertex = new Vertex(vec3(m.positions[i*3],m.positions[i*3+1],m.positions[i*3+2]));
            vec3 n = vec3(m.normals[i*3],m.normals[i*3+1],m.normals[i*3+2]);
            if (abs(n.x) == 0.0 && abs(n.y) == 0.0 && abs(n.z) == 0.0 ) {
                n = vec3(0.0001, 0.0001, 0.0001);
                zeroNormals++;
            }
            newVertex->normal = glm::normalize(n);

            if (m.texcoords.size() != 0)
                newVertex->texture = vec2(m.texcoords[i*2],m.texcoords[i*2+1]);
            ORIGVERTICES.push_back(newVertex);
        }
        printf("Replaced %d zeroed Normals\n", zeroNormals);
        for (int i = 0; i < m.indices.size(); i++)
            ORIGINDICES.push_back((int)m.indices[i]);
    }

    void generateQuantizedVertices(vector<Vertex*>& ORIGVERTICES, AABB* aabb, int bV, int bT, vector<Vertex*>& VERTICES) {

        //quantize everything
        for (int i = 0; i < ORIGVERTICES.size(); i++){
            Vertex * currVert = ORIGVERTICES[i];
            vector<int> currPos = Geo::quantizeVertexPosition(currVert->pos, aabb, bV);

            Vertex *newVertex = new Vertex(vec3(currPos[0], currPos[1], currPos[2]));
            newVertex->normal2 = Geo::octEncode_8bit(currVert->normal);

            vector<int> currCoords = Geo::quantizeVertexTexture(currVert->texture, bT);
            newVertex->texture = vec2(currCoords[0],currCoords[1]);
            VERTICES.push_back(newVertex);

        }
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

            Face *newFace = new Face(newFaceIndices[0], newFaceIndices[1], newFaceIndices[2]);
            indices_out.push_back(newFaceIndices[0]);
            indices_out.push_back(newFaceIndices[1]);
            indices_out.push_back(newFaceIndices[2]);
            if (newFaceIndices[0] < 0)
                cout << newFaceIndices[0] << " " << newFaceIndices[1] << " " << newFaceIndices[2] << endl;
            //FACES.push_back(newFace);
        }

        map<int, Vertex*> INT_VERTEX_MAP;
        //now flip the map
        for (auto itr = vertex_int_map.begin(); itr != vertex_int_map.end(); itr++) {
            INT_VERTEX_MAP[itr->second] = (Vertex*)&(itr->first);
        }

        for (int i; i < INT_VERTEX_MAP.size(); i++){
            FINAL_VERTICES.push_back(INT_VERTEX_MAP[i]);
        }

    }

    void add_tri(std::vector<int>& out_inds, int a, int b, int c)
    {
        assert(a >= b);
        out_inds.push_back(a);
        out_inds.push_back(b);
        out_inds.push_back(c);
    }

    void add_double_tri(std::vector<int>& out_inds, int a, int b, int c, int d)
    {
        assert(a < b);
        out_inds.push_back(a);
        out_inds.push_back(b);
        out_inds.push_back(c);
        out_inds.push_back(d);
    }

    bool try_merge_with_next(std::vector<int>& out_inds, const std::vector<int>& inds, size_t base)
    {
        // is there even a next tri?
        if (base + 3 >= inds.size())
            return false;

        // is this tri degenerate?
        const int *tri = &inds[base];
        if (tri[0] == tri[1] || tri[1] == tri[2] || tri[2] == tri[0])
            return false;

        // does the next tri contain the opposite of at least one
        // of our edges?
        const int *next = &inds[base + 3];

        // go through 3 edges of tri
        for (int i = 0; i < 3; i++)
        {
            // try to find opposite of edge ab, namely ba.
            int a = tri[i];
            int b = tri[(i + 1) % 3];
            int c = tri[(i + 2) % 3];

            for (int j = 0; j < 3; j++)
            {
                if (next[j] == b && next[(j + 1) % 3] == a)
                {
                    int d = next[(j + 2) % 3];

                    if (a < b)
                        add_double_tri(out_inds, a, b, c, d);
                    else // must be c > a, since we checked that a != c above; this ends up swapping two tris.
                        add_double_tri(out_inds, b, a, d, c);

                    return true;
                }
            }
        }

        return false;
    }

    void compressIndexBuffer(vector<int>& inds_in, vector<int>&inds_out) {
        for (size_t base = 0; base < inds_in.size(); ) {
            if (try_merge_with_next(inds_out, inds_in, base))
                base += 6; // packed two triangles
            else {
                const int *tri = &inds_in[base];
                if (tri[0] >= tri[1])
                    add_tri(inds_out, tri[0], tri[1], tri[2]);
                else if (tri[1] >= tri[2])
                    add_tri(inds_out, tri[1], tri[2], tri[0]);
                else
                {
                    // must have tri[2] >= tri[0],
                    // otherwise we'd have tri[0] < tri[1] < tri[2] < tri[0] (contradiction)
                    add_tri(inds_out, tri[2], tri[0], tri[1]);
                }
                base += 3;
            }
        }
    }

    int encodeIndicesDelta(vector<int>& FINAL_INDICES, vector<u_int_32>& indices_to_write) {

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

        cout << "num fourbyters: " << fourByters << endl;

        cout << "average store: " << (al/un) << endl;
        return fourByters;
    }

    int encodeIndicesHighWater(vector<int>& FINAL_INDICES, vector<u_int_32>& indices_to_write, int max_step, bool utfSafe) {
        //hwm encode indices
        float al = 0;
        float un = 0;
        int fourByters = 0;

        int toStore = 0; int hi = max_step-1; int index = 0;

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


        cout << "max store: " << maxStore << endl;
        cout << "num fourbyters: " << fourByters << endl;
        cout << "average store: " << (al/un) << endl;

        return fourByters;
    }

    void deltaEncodeVertices(vector<Vertex*>& FINAL_VERTICES, vector<int>& VTXDelta, int bT) {
        int lastPosX = 0, lastPosY = 0, lastPosZ = 0,
                lastNormX = 0, lastNormY = 0,
                lastCoordU = 0, lastCoordV = 0, delta = 0;

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
        for (int i = 0; i < FINAL_VERTICES.size(); i++) {
            delta = (int)(FINAL_VERTICES[i]->normal2.x - lastNormX);
            lastNormX = (int)(FINAL_VERTICES[i]->normal2.x);
            VTXDelta.push_back(Geo::getInterleavedUint(delta));
        }
        for (int i = 0; i < FINAL_VERTICES.size(); i++) {
            delta = (int)(FINAL_VERTICES[i]->normal2.y - lastNormY);
            lastNormY = (int)(FINAL_VERTICES[i]->normal2.y);
            VTXDelta.push_back(Geo::getInterleavedUint(delta));
        }
        if (bT > 0) {
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

    void writeDataUTF(vector<int>& vertices_to_write,
                      vector<u_int_32>& indices_to_write,
                      const string OUT_ROOT,
                      IndexCoding indCoding) {
        //write files
        cout << "Writing data..." << endl;

        //first write utfFile (in order to get it's size for JSON
        FILE * utfFile;
        string outUTF8Name = OUT_ROOT + ".utf8";
        utfFile = fopen (outUTF8Name.c_str(),"w");
        if (utfFile!=NULL) {
            for (int i = 0; i < vertices_to_write.size(); i++) {

                //write_utf8(vertices_to_write[i], utfFile);
            }

            for (int i = 0; i < indices_to_write.size(); i++) {
                write_utf8(indices_to_write[i], utfFile);
            }

            fclose (utfFile);


        }

        struct stat filestatus;
        stat( outUTF8Name.c_str(), &filestatus );
        cout << int(filestatus.st_size)/1000 << " bytes\n";
    }


    void writeDataPNG(vector<int>& vertices_to_write,
                    vector<u_int_32>& indices_to_write,
                    const string OUT_ROOT,
                    IndexCoding indCoding) {

        const int IMAGE_RESOLUTION = 4096;

        png::image< png::rgb_pixel > imagei(IMAGE_RESOLUTION, IMAGE_RESOLUTION);

        int vCounter = 0;
        int iCounter = 0;

        for (size_t y = 0; y < imagei.get_height(); ++y) {
            for (size_t x = 0; x < imagei.get_width(); ++x) {

                if (vCounter < vertices_to_write.size()){
                    unsigned int quantVal = vertices_to_write[vCounter++];

                    int r = (quantVal & 0x00FF0000) >> 16;
                    int g = (quantVal & 0x0000FF00) >> 8;
                    int b = (quantVal & 0x000000FF);
                    imagei[y][x] = png::rgb_pixel(r, g, b);

                }
                else if (iCounter < indices_to_write.size()){

                    unsigned int quantVal = indices_to_write[iCounter++];
                    int r = (quantVal & 0x00FF0000) >> 16;
                    int g = (quantVal & 0x0000FF00) >> 8;
                    int b = (quantVal & 0x000000FF);
                    imagei[y][x] = png::rgb_pixel(r, g, b);
                }

            }
        }
        imagei.write(OUT_ROOT + ".png");

    }


    void writeHeader(vector<int>& vertices_to_write,
                     vector<u_int_32>& indices_to_write,
                     int numVerts,
                     int numFaces,
                     int bV,
                     int bT,
                     int fourByters,
                     const string OUT_ROOT,
                     AABB* aabb,
                     MeshEncoding coding,
                     IndexCoding indCoding,
                     IndexCompression indCompression,
                     int max_hw_step) {

        json metadata;
        json lodArray(json::an_array);

        json currLod;
        currLod["vertices"] = numVerts;
        currLod["faces"] = numFaces;
        currLod["utfIndexBuffer"] = int(indices_to_write.size())+fourByters; //add number of 3 or 4 byte indices for utf parsing

        json currBits;
        currBits["vertex"] = bV;
        currBits["normal"] = 8;
        currBits["texture"] = bT;
        currLod["bits"] = move(currBits);
        lodArray.add(currLod);

        metadata["LOD"] = move(lodArray);

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

        if (indCompression == IndexCompression::PAIRED_TRIS)
            metadata["indexCompression"] = "pairedtris";
        else
            metadata["indexCompression"] = "none";

        if (coding == MeshEncoding::UTF) {
            string outUTF8Name = OUT_ROOT + ".utf8";
            struct stat filestatus;
            stat(outUTF8Name.c_str(), &filestatus);
            metadata["data"] = outUTF8Name;
            if (indCoding == IndexCoding::DELTA){
                metadata["indexCoding"] = "delta";
            } else {
                metadata["indexCoding"] = "highwater";
                metadata["max_step"] = max_hw_step;
            }

            metadata["size"] = int(filestatus.st_size) / 1000;
        }
        else if (coding == MeshEncoding::PNG) {
            string outPNGName = OUT_ROOT + ".png";
            struct stat filestatus;
            stat(outPNGName.c_str(), &filestatus);
            metadata["data"] = outPNGName;
            if (indCoding == IndexCoding::DELTA){
                metadata["indexCoding"] = "delta";
            } else {
                metadata["indexCoding"] = "highwater";
                metadata["max_step"] = max_hw_step;
            }
            metadata["size"] = int(filestatus.st_size) / 1000;
        }
        ofstream jsonFile;
        jsonFile.open (OUT_ROOT + ".json");
        jsonFile << metadata;
        jsonFile.close();
    }


    void exportMesh(const string INFILEPATH, const string OUT_ROOT, int bV, int bT,
                    MeshEncoding coding, IndexCoding indCoding, IndexCompression indCompression) {



        vector<Vertex*> quantizedVertices;

        //read OBJ file
        vector<Vertex*> origVertices;
        vector<int> origIndices;
        readOBJ(INFILEPATH, origVertices, origIndices);

//        int* tip = (int*)tipsify(&origIndices[0], origIndices.size()/3, origVertices.size(), 100);
//        vector<int> reorder_indices(tip, tip+origIndices.size());
//
        int *forth = (int*)reorderForsyth(&origIndices[0], origIndices.size()/3, origVertices.size());
        vector<int> reorder_indices(forth, forth+origIndices.size());


        //get AABB
        AABB* aabb = Geo::getAABB(origVertices);

        //quantize
        generateQuantizedVertices(origVertices, aabb, bV, bT, quantizedVertices);


        //reIndex data (for future progressive stuff)
        map<Vertex, int> vertex_int_map;
        vector<Vertex*> FINAL_VERTICES;
        vector<int> reIndexed_inds;

        cout << "Reindexing..." << endl;
        reIndexAccordingToIndices(quantizedVertices, reorder_indices, vertex_int_map, FINAL_VERTICES, reIndexed_inds);


        vector<int> FINAL_INDICES;
        int max_step = 1;
        if (indCompression == IndexCompression::PAIRED_TRIS) {
            cout << "Compressing indices..." << endl;
            compressIndexBuffer(reIndexed_inds, FINAL_INDICES);
            max_step = 3;
        }
        else
            FINAL_INDICES = reIndexed_inds;


        cout << "Compressing..." << endl;
        vector<int> vertices_to_write;
        deltaEncodeVertices(FINAL_VERTICES, vertices_to_write, bT);
        vector<u_int_32> indices_to_write;
        int fourByters = 0;

        if (indCoding == IndexCoding::DELTA){
            fourByters = encodeIndicesDelta(FINAL_INDICES, indices_to_write);
        }
        else {

            if (coding == MeshEncoding::UTF)
                fourByters = encodeIndicesHighWater(FINAL_INDICES, indices_to_write, max_step, true); //true says its UTF safe
            else
                encodeIndicesHighWater(FINAL_INDICES, indices_to_write, max_step, false); //true says its UTF safe
        }



        //write
        cout << "Writing " << vertices_to_write.size() << " vertex data and " << indices_to_write.size() << " index data." << endl;

        if (coding == MeshEncoding::UTF)
            writeDataUTF(vertices_to_write, indices_to_write, OUT_ROOT, indCoding);
        else
            writeDataPNG(vertices_to_write, indices_to_write, OUT_ROOT, indCoding);
        writeHeader(vertices_to_write,
                      indices_to_write,
                      int(FINAL_VERTICES.size()),
                      int(FINAL_INDICES.size()/3),
                      bV,
                      bT,
                      fourByters,
                      OUT_ROOT,
                      aabb,
                      coding,
                      indCoding,
                      indCompression,
                      max_step);
    }

}

