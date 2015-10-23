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
#include <string.h>
#include <stdlib.h>

#define EPSILON 0.0001

#ifdef putc_unlocked
# define PutChar putc_unlocked
#else
# define PutChar putc
#endif  // putc_unlocked

namespace Export {

    int write_varint(unsigned value, FILE* file) {
        if (value < 0x80) {
            PutChar(static_cast<char>(value), file);
            return 1;

        } else if( value < 0x4000) {

            unsigned char firstByte = (static_cast<char>(value) & 0x7F); // first seven bits
            firstByte |= 1 << 7;
            PutChar(static_cast<char>(firstByte), file);

            unsigned shiftVal = value >> 7; //shift off first 7 bits
            char secondByte = (static_cast<char>(shiftVal) & 0x7F); // next seven bits
            PutChar(static_cast<char>(secondByte), file);
            return 2;

        } else if (value < 0x200000) {
            unsigned char firstByte = (static_cast<char>(value) & 0x7F); // first seven bits
            firstByte |= 1 << 7;
            PutChar(static_cast<char>(firstByte), file);

            unsigned shiftVal = value >> 7; //shift off first 7 bits
            unsigned char secondByte = (static_cast<char>(shiftVal) & 0x7F); // next seven bits
            secondByte |= 1 << 7;
            PutChar(static_cast<char>(secondByte), file);

            shiftVal = value >> 14; //shift off first 14 bits
            unsigned char thirdByte = (static_cast<char>(shiftVal) & 0x7F); // next seven bits
            PutChar(static_cast<char>(thirdByte), file);
            return 3;
        } else if (value < 0x10000000){
            cout << "invalid b128 value " << value << endl;
            return 0;
        }
        return 0;
    }

    void write_utf8(unsigned code_point, FILE * utfFile)
    {
        if (code_point < 0x80) {
            PutChar(static_cast<char>(code_point), utfFile);
        } else if (code_point <= 0x7FF) {
            PutChar(static_cast<char>((code_point >> 6) + 0xC0), utfFile);
            PutChar(static_cast<char>((code_point & 0x3F) + 0x80), utfFile);
        } else if (code_point <= 0xFFFF) {
            PutChar(static_cast<char>((code_point >> 12) + 0xE0), utfFile);
            PutChar(static_cast<char>(((code_point >> 6) & 0x3F) + 0x80), utfFile);
            PutChar(static_cast<char>((code_point & 0x3F) + 0x80), utfFile);
        } else if (code_point <= 0x10FFFF) {
            PutChar(static_cast<char>((code_point >> 18) + 0xF0), utfFile);
            PutChar(static_cast<char>(((code_point >> 12) & 0x3F) + 0x80), utfFile);
            PutChar(static_cast<char>(((code_point >> 6) & 0x3F) + 0x80), utfFile);
            PutChar(static_cast<char>((code_point & 0x3F) + 0x80), utfFile);
        } else {

            cout << "invalid utf8 code_point " << code_point << endl;
        }
    }

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

    void generateQuantizedVertices(vector<Vertex*>& ORIGVERTICES,
                                   AABB* aabb, int bV, int bT,
                                   vector<Vertex*>& VERTICES,
                                    VertexQuantization vertexQuantization,
                                    vector<int>& out_bV) {

        int newVertCounter = 0;
        map<Vertex, int> vertex_int_map;
        map<int, int> int_int_map; //old->new
        std::map<Vertex, int>::iterator it;

        int bVx, bVy, bVz;
        bVx = bVy = bVz = 0;
        // if we quantize per axis, calculate if can save
        if (vertexQuantization == VertexQuantization::PER_AXIS) {
            //find longest axis; 0 = x, 1 = y, 2 = z
            int longAxis = 0;
            if (aabb->range.y > aabb->range.x && aabb->range.y > aabb->range.z )
                longAxis = 1;
            else if (aabb->range.z > aabb->range.x && aabb->range.z > aabb->range.y)
                longAxis = 2;

            if (longAxis == 0) {
                bVx = bV;
                int intFactorY = (int) (aabb->range.x / aabb->range.y);
                bVy = bV - (intFactorY - 1);
                int intFactorZ = (int) (aabb->range.x / aabb->range.z);
                bVz = bV - (intFactorZ - 1);
            }
            if (longAxis == 1) {
                int intFactorX = (int) (aabb->range.y / aabb->range.x);
                bVx = bV - (intFactorX - 1);
                bVy = bV;
                int intFactorZ = (int) (aabb->range.y / aabb->range.z);
                bVz = bV - (intFactorZ - 1);
            }
            if (longAxis == 0) {

                int intFactorX = (int) (aabb->range.z / aabb->range.x);
                bVx = bV - (intFactorX - 1);
                int intFactorY = (int) (aabb->range.z / aabb->range.y);
                bVy = bV - (intFactorY - 1);
                bVz = bV;
            }

            printf("long axis %d\n", longAxis);
            printf("bVx %d, bVy %d, bVz %d\n", bVx, bVy, bVz);
            out_bV.push_back(bVx);
            out_bV.push_back(bVy);
            out_bV.push_back(bVz);
        } else {
            out_bV.push_back(bV);
            out_bV.push_back(bV);
            out_bV.push_back(bV);
        }



        //quantize everything
        for (int i = 0; i < ORIGVERTICES.size(); i++){
            Vertex * currVert = ORIGVERTICES[i];
            vector<int> currPos;
            if (vertexQuantization == VertexQuantization::GLOBAL)
                currPos = Geo::quantizeVertexPosition(currVert->pos, aabb, bV);
            else if (vertexQuantization == VertexQuantization::PER_AXIS)
                currPos = Geo::quantizeVertexPosition(currVert->pos, aabb, bVx, bVy, bVz);
            else { printf("Unknown vertex quantization method"); exit(0);}

            Vertex *newVertex = new Vertex(vec3(currPos[0], currPos[1], currPos[2]));
            newVertex->normal2 = Geo::octEncode_8bit(currVert->normal);

            vector<int> currCoords = Geo::quantizeVertexTexture(currVert->texture, bT);
            newVertex->texture = vec2(currCoords[0],currCoords[1]);

            VERTICES.push_back(newVertex);

        }
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

        printf("pre quantize verts: %d ; post %d\n", (int)in_vertices.size(), (int)out_vertices.size());

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

        printf("pre quantize inds: %d ; post %d\n", (int)in_indices.size(), (int)out_indices.size());

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

        //now remove duplicate tris from index

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

        cout << "num fourbyters: " << fourByters << endl;

        cout << "average store: " << (al/un) << endl;
        return fourByters;
    }

    int encodeIndicesHighWater(vector<int>& FINAL_INDICES, vector<int>& indices_to_write, int max_step, bool utfSafe) {
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
                      vector<int>& indices_to_write,
                      const string OUT_ROOT,
                      IndexCoding indCoding) {

        //write files
        cout << "Writing data..." << endl;

        //first write utfFile (in order to get it's size for JSON
        FILE * utfFile;
        string outUTF8Name = OUT_ROOT + ".utf8";
        utfFile = fopen (outUTF8Name.c_str(),"w");
        if (utfFile!=NULL) {
            int counter = 0;
            for (int i = 0; i < vertices_to_write.size(); i++) {

                write_utf8(vertices_to_write[i], utfFile);
                counter++;
            }

            for (int i = 0; i < indices_to_write.size(); i++) {
                if (counter > 319560 && counter < 319575)
                    printf("%d ", indices_to_write[i]);
                write_utf8(indices_to_write[i], utfFile);
                counter++;
            }

            fclose (utfFile);


        }

        struct stat filestatus;
        stat( outUTF8Name.c_str(), &filestatus );
        cout << int(filestatus.st_size)/1000 << " bytes\n";
    }


    void writeDataPNG(vector<int>& vertices_to_write,
                    vector<int>& indices_to_write,
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

    int writeDataVARINT(vector<int>& vertices_to_write,
                      vector<int>& indices_to_write,
                      const string OUT_ROOT,
                      IndexCoding indCoding) {

        //write files
        cout << "Writing varint data..." << endl;

        //first write utfFile (in order to get it's size for JSON
        FILE * vintFile;
        string outVINTName = OUT_ROOT + ".b128";
        vintFile = fopen (outVINTName.c_str(),"w");
        int numBytes = 0;
        if (vintFile!=NULL) {
            for (int i = 0; i < vertices_to_write.size(); i++) {


                numBytes+=write_varint(vertices_to_write[i], vintFile);
            }

            for (int i = 0; i < indices_to_write.size(); i++) {
                numBytes+=write_varint(indices_to_write[i], vintFile);
            }

            fclose (vintFile);


        }

        struct stat filestatus;
        stat( outVINTName.c_str(), &filestatus );
        cout << int(filestatus.st_size)/1000 << " bytes\n";
        return numBytes;
    }


    void writeHeader(vector<int>& vertices_to_write,
                     vector<int>& indices_to_write,
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
                     int max_hw_step,
                     int numBytes,
                     VertexQuantization vertexQuantization,
                     int bVx = 11,
                     int bVy = 11,
                     int bVz = 11) {

        json metadata;
        json lodArray(json::an_array);

        json currLod;
        currLod["vertices"] = numVerts;
        currLod["faces"] = numFaces;
        currLod["utfIndexBuffer"] = int(indices_to_write.size())+fourByters; //add number of 3 or 4 byte indices for utf parsing

        json currBits;
        if (vertexQuantization == VertexQuantization::GLOBAL) {
            currLod["vertexQuantization"] = "global";
            currBits["vertex"] = bV;
        }
        else {
            currLod["vertexQuantization"] = "perVertex";
            json currBitsVTX;
            currBitsVTX["x"] = bVx;
            currBitsVTX["y"] = bVy;
            currBitsVTX["z"] = bVz;
            currBits["vertex"] = currBitsVTX;
        }
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
        else if (coding == MeshEncoding::VARINT) {
            string outVARINTName = OUT_ROOT + ".b128";
            struct stat filestatus;
            stat(outVARINTName.c_str(), &filestatus);
            metadata["data"] = outVARINTName;
            if (indCoding == IndexCoding::DELTA){
                metadata["indexCoding"] = "delta";
            } else {
                metadata["indexCoding"] = "highwater";
                metadata["max_step"] = max_hw_step;
            }
            metadata["size"] = int(filestatus.st_size) / 1000;
            metadata["numbytes"] = numBytes;
        }
        ofstream jsonFile;
        jsonFile.open (OUT_ROOT + ".json");
        jsonFile << metadata;
        jsonFile.close();
    }


    void exportMesh(const string INFILEPATH, const string OUT_ROOT, int bV, int bT,
                    MeshEncoding coding, IndexCoding indCoding,
                    IndexCompression indCompression,
                    TriangleReordering triOrdering,
                    VertexQuantization vertexQuantization) {



        vector<Vertex*> quantizedVertices;

        //read OBJ file
        vector<Vertex*> origVertices;
        vector<int> origIndices;
        readOBJ(INFILEPATH, origVertices, origIndices);

        vector<int> reorder_indices;
        if (triOrdering == TriangleReordering::TIPSIFY) {
            int* tip = (int*)tipsify(&origIndices[0], origIndices.size()/3, origVertices.size(), 100);
            reorder_indices.assign(tip, tip+origIndices.size());
        }
        else if (triOrdering == TriangleReordering::FORSYTH) {
            int *forth = (int *) reorderForsyth(&origIndices[0], origIndices.size() / 3, origVertices.size());
            reorder_indices.assign(forth, forth + origIndices.size());
        }
        else
            reorder_indices = origIndices;


        //get AABB
        AABB* aabb = Geo::getAABB(origVertices);


        //quantize
        vector<int> out_bV;
        generateQuantizedVertices(origVertices, aabb, bV, bT, quantizedVertices, vertexQuantization, out_bV);

        //remove duplicate verts
        vector<Vertex*> dupVerts;
        vector<int> dupInds;
        removeDuplicateVertices(quantizedVertices,
                                dupVerts,
                                reorder_indices,
                                dupInds);



        //reIndex data (for future progressive stuff)
        map<Vertex, int> vertex_int_map;
        vector<Vertex*> FINAL_VERTICES;
        vector<int> reIndexed_inds;

        cout << "Reindexing..." << endl;
        //reIndexAccordingToIndices(quantizedVertices, reorder_indices, vertex_int_map, FINAL_VERTICES, reIndexed_inds);
        reIndexAccordingToIndices(dupVerts, dupInds, vertex_int_map, FINAL_VERTICES, reIndexed_inds);


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
        vector<int> indices_to_write;
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

        int numBytes = 0;
        if (coding == MeshEncoding::UTF)
            writeDataUTF(vertices_to_write, indices_to_write, OUT_ROOT, indCoding);
        else if (coding == MeshEncoding::PNG)
            writeDataPNG(vertices_to_write, indices_to_write, OUT_ROOT, indCoding);
        else if (coding == MeshEncoding::VARINT)
            numBytes = writeDataVARINT(vertices_to_write, indices_to_write, OUT_ROOT, indCoding);

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
                      max_step,
                      numBytes,
                      vertexQuantization,
                      out_bV[0], out_bV[1], out_bV[2]);
    }

}

