//
// Created by Alun on 16/07/2015.
//

#include "Export.h"
#include <png++/png.hpp>
#include <sys/types.h>
#include <sys/stat.h>
#include "tipsify.cpp"
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
                                   vector<int>& FINAL_INDICES) {
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
            FINAL_INDICES.push_back(newFaceIndices[0]);
            FINAL_INDICES.push_back(newFaceIndices[1]);
            FINAL_INDICES.push_back(newFaceIndices[2]);
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

    int encodeIndicesHighWater(vector<int>& FINAL_INDICES, vector<int>& indices_to_write) {
        //hwm encode indices
        float al = 0;
        float un = 0;
        int index = 0;
        int fourByters = 0;
        int nextHighWaterMark = 0;
        int toStore = 0;
        for (int i = 0; i < FINAL_INDICES.size(); i++) {

            index = FINAL_INDICES[i];

            toStore = (nextHighWaterMark-index);
            al+=toStore;
            un++;

            if (toStore > 65535) //count the number of indices which need more than 16-bit to store
                fourByters++;

            pushSafeUTF(toStore, indices_to_write);

            if (index == nextHighWaterMark)
                nextHighWaterMark++;

        }

        cout << "num fourbyters: " << fourByters << endl;

        cout << "average store: " << (al/un) << endl;

        return fourByters;
    }

    void encodeAndWriteUTF(vector<Vertex*>& FINAL_VERTICES,
                           vector<int>& FINAL_INDICES,
                           int bV,
                           int bT,
                           const string OUT_ROOT,
                           AABB* aabb) {
        vector<int> VTXPosX;
        vector<int> VTXPosY;
        vector<int> VTXPosZ;
        vector<int> VTXNormX;
        vector<int> VTXNormY;
        vector<int> VTXCoordU;
        vector<int> VTXCoordV;
        int lastPosX = 0, lastPosY = 0, lastPosZ = 0,
                lastNormX = 0, lastNormY = 0,
                lastCoordU = 0, lastCoordV = 0, delta = 0;
        for (int i = 0; i < FINAL_VERTICES.size(); i++) {

            Vertex * currVert = FINAL_VERTICES[i];

            vec3 currPos = currVert->pos;
            vec2 currNorm = currVert->normal2;
            vec2 currCoords = currVert->texture;

            delta = (int)(currPos.x - lastPosX);
            lastPosX = (int)currPos.x;
            VTXPosX.push_back(Geo::getInterleavedUint(delta));

            delta = (int)(currPos.y - lastPosY);
            lastPosY = (int)currPos.y;
            VTXPosY.push_back(Geo::getInterleavedUint(delta));

            delta = (int)(currPos.z - lastPosZ);
            lastPosZ = (int)currPos.z;
            VTXPosZ.push_back(Geo::getInterleavedUint(delta));

            delta = (int)(currNorm.x - lastNormX);
            lastNormX = (int)currNorm.x;
            VTXNormX.push_back(Geo::getInterleavedUint(delta));

            delta = (int)(currNorm.y - lastNormY);
            lastNormY = (int)currNorm.y;
            VTXNormY.push_back(Geo::getInterleavedUint(delta));

            if (bT > 0) {
                delta = (int)(currCoords.x - lastCoordU);
                lastCoordU = (int)currCoords.x;
                VTXCoordU.push_back(Geo::getInterleavedUint(delta));

                delta = (int)(currCoords.y - lastCoordV);
                lastCoordV = (int)currCoords.y;
                VTXCoordV.push_back(Geo::getInterleavedUint(delta));
            }
        }

        vector<int> indices_to_write;
        int fourByters = encodeIndicesHighWater(FINAL_INDICES, indices_to_write);

        //write files
        cout << "Writing files..." << endl;

        //first write utfFile (in order to get it's size for JSON
        FILE * utfFile;
        string outUTF8Name = OUT_ROOT + ".utf8";
        utfFile = fopen (outUTF8Name.c_str(),"w");
        if (utfFile!=NULL) {
            for (int i = 0; i < VTXPosX.size(); i++) {
                int bob = VTXPosX[i];
                write_utf8(VTXPosX[i], utfFile);
            }
            for (int i = 0; i < VTXPosY.size(); i++) {
                int bob = VTXPosX[i];
                write_utf8(VTXPosY[i], utfFile);
            }
            for (int i = 0; i < VTXPosZ.size(); i++) {
                int bob = VTXPosZ[i];
                write_utf8(VTXPosZ[i], utfFile);
            }

            for (int i = 0; i < VTXNormX.size(); i++) {
                write_utf8(VTXNormX[i], utfFile);
            }
            for (int i = 0; i < VTXNormY.size(); i++) {
                write_utf8(VTXNormY[i], utfFile);
            }

            for (int i = 0; i < VTXCoordU.size(); i++) {
                write_utf8(VTXCoordU[i], utfFile);
            }
            for (int i = 0; i < VTXCoordV.size(); i++) {
                write_utf8(VTXCoordV[i], utfFile);
            }
            for (int i = 0; i < indices_to_write.size(); i++) {
                write_utf8(indices_to_write[i], utfFile);
            }
            fclose (utfFile);


        }


        struct stat filestatus;
        stat( outUTF8Name.c_str(), &filestatus );
        cout << int(filestatus.st_size)/1000 << " bytes\n";


        json metadata;
        json lodArray(json::an_array);

        json currLod;
        currLod["vertices"] = int(FINAL_VERTICES.size());
        currLod["faces"] = int(FINAL_INDICES.size()/3);
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
        metadata["data"] = outUTF8Name;
        metadata["size"] = int(filestatus.st_size)/1000;

        ofstream jsonFile;
        jsonFile.open (OUT_ROOT + ".json");
        jsonFile << metadata;
        jsonFile.close();
    }


    void exportUTFMesh(const string INFILEPATH, const string OUT_ROOT, int bV, int bT) {



        vector<Vertex*> quantizedVertices;

        //read OBJ file
        vector<Vertex*> origVertices;
        vector<int> origIndices;
        readOBJ(INFILEPATH, origVertices, origIndices);

        int* tip = (int*)tipsify(&origIndices[0], origIndices.size()/3, origVertices.size(), 100);

        vector<int> tips_indices(tip, tip+origIndices.size());

        //get AABB
        AABB* aabb = Geo::getAABB(origVertices);

        //quantize
        generateQuantizedVertices(origVertices, aabb, bV, bT, quantizedVertices);



        map<Vertex, int> vertex_int_map;
        vector<Vertex*> FINAL_VERTICES;
        vector<int> FINAL_INDICES;

        cout << "Reindexing..." << endl;
        reIndexAccordingToIndices(quantizedVertices, tips_indices, vertex_int_map, FINAL_VERTICES, FINAL_INDICES);



//        for (int i = 0; i < 100; i++) {
//            printf("%d, %d\n", FINAL_INDICES[i], TIPS_INDICES[i]);
//        }


        cout << "Compressing..." << endl;
        encodeAndWriteUTF(FINAL_VERTICES, FINAL_INDICES, bV, bT, OUT_ROOT, aabb);



    }



/*


    vector<unsigned int> loadAndQuantize(const string INFILEPATH,
                         const string OUT_ROOT,
                         vector<Vertex*>& QUANTVERTICES,
                          AABB* aabb,
                         int bV, int bN, int bT){

        vector<Vertex*> ORIGVERTICES;

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
        for (int i = 0; i < m.positions.size()/3; i++) {
            Vertex *newVertex = new Vertex(vec3(m.positions[i*3],m.positions[i*3+1],m.positions[i*3+2]));
            newVertex->normal = vec3(m.normals[i*3],m.normals[i*3+1],m.normals[i*3+2]);
            if (m.texcoords.size() != 0)
                newVertex->texture = vec2(m.texcoords[i*2],m.texcoords[i*2+1]);
            ORIGVERTICES.push_back(newVertex);
        }


        //get AABB
        aabb = Geo::getAABB(ORIGVERTICES);

        //quantize everything
        for (int i = 0; i < ORIGVERTICES.size(); i++){
            Vertex * currVert = ORIGVERTICES[i];
            vector<int> currPos = (vector<int>)Geo::quantizeVertexPosition(currVert->pos, aabb, bV);
            vector<int> currNorm = (vector<int>)Geo::quantizeVertexNormal(currVert->normal, bN);


            Vertex *newVertex = new Vertex(vec3(currPos[0], currPos[1], currPos[2]));
            newVertex->normal = vec3(currNorm[0],currNorm[1],currNorm[2]);
            vector<int> currCoords = (vector<int>)Geo::quantizeVertexTexture(currVert->texture, bT);
            newVertex->texture = vec2(currCoords[0],currCoords[1]);
            QUANTVERTICES.push_back(newVertex);

        }
        return m.indices;
    }


    void reIndex(vector<Vertex*>& QUANTVERTICES,
                 vector<unsigned int>& mindices,
                 vector<Face*>& FACES,
                 vector<Vertex*>& FINALVERTICES){

        map<Vertex, int> VERTICES_MAP;
        //reindex
        int newVertCounter = 0;
        std::map<Vertex, int>::iterator it;
        int index = 0;
        int nextHighWaterMark = 0, delta = 0;
        //for each set of three indices
        for (int i = 0; i < mindices.size(); i+=3) {
            //creat empty vector for new face indices
            vector<int> newFaceIndices;
            //loop through next three indices
            for (int j = 0; j < 3; j++) {
                //get current index
                int index = mindices[i+j];
                //get vert referenced by pointer
                Vertex vert = *QUANTVERTICES[index];
                //try to find vertex in map
                it = VERTICES_MAP.find(vert);

                //if its there, add the index to the newFaceindex list
                if (it != VERTICES_MAP.end()) {

//                    delta = newVertCounter - it->second;
//                    if (delta > 255) {
//
//                        newFaceIndices.push_back(newVertCounter);
//                        VERTICES_MAP[vert] = newVertCounter;
//                        FINALVERTICES.push_back(QUANTVERTICES[index]);
//                        newVertCounter++; // now increment
//                    }
//                    else {
                    newFaceIndices.push_back(it->second);
                    //}

                }
                    //otherwise, add size-1 of current map as index
                    //and add vert to map
                else {
                    newFaceIndices.push_back(newVertCounter);
                    VERTICES_MAP[vert] = newVertCounter;
                    FINALVERTICES.push_back(QUANTVERTICES[index]);
                    newVertCounter++; // now increment
                }
            }

            Face *newFace = new Face(newFaceIndices[0], newFaceIndices[1], newFaceIndices[2]);
            if (newFaceIndices[0] < 0)
                cout << newFaceIndices[0] << " " << newFaceIndices[1] << " " << newFaceIndices[2] << endl;
            FACES.push_back(newFace);


        }

    }

    void exportPNGMesh(const string INFILEPATH, const string OUT_ROOT) {

        int bV = 11;
        int bN = 8;
        int bT = 8;

        vector<Vertex*> QUANTVERTICES;
        vector<Vertex*> FINALVERTICES;
        vector<Face*> FACES;
        AABB* aabb;

        cout << "Reading files..." << endl;

        vector<unsigned int> mindices = loadAndQuantize(INFILEPATH, OUT_ROOT, QUANTVERTICES, aabb, bV, bN, bT);

        cout << "Reindexing..." << endl;

        reIndex(QUANTVERTICES, mindices, FACES, FINALVERTICES);


        //delta encode indices
        vector<int> INDICES;
        int lastIndex = 0;
        int index = 0;
        int nextHighWaterMark = 0;
        int delta;
        int dMax = 0;
        for (int i = 0; i < FACES.size(); i++) {
            for (int j = 0; j < FACES[i]->indices.size(); j++) {

                index = FACES[i]->indices[j];
                delta = nextHighWaterMark - index;
                if (index == nextHighWaterMark) {
                    nextHighWaterMark++;
                }

                if (delta > dMax) dMax = delta;

                INDICES.push_back(delta);
            }
        }

        cout << "max:" << dMax << endl;

        cout << QUANTVERTICES.size() << endl;
        cout << FINALVERTICES.size() << endl;
        cout << INDICES.size() << endl;



        //embed all position into one vector using delta encoding
        vector<int> positionData;
        //do all xs, then all ys, then all zs in order to minimise delta
        for (int i = 0; i < FINALVERTICES.size(); i++) {
            int newIndex = FINALVERTICES[i]->pos.x;
            positionData.push_back(newIndex);
        }
        //do all xs, then all ys, then all zs in order to minimise delta
        for (int i = 0; i < FINALVERTICES.size(); i++) {
            int newIndex = FINALVERTICES[i]->pos.y;
            positionData.push_back(newIndex);
        }
        //do all xs, then all ys, then all zs in order to minimise delta
        for (int i = 0; i < FINALVERTICES.size(); i++) {
            int newIndex = FINALVERTICES[i]->pos.z;
            positionData.push_back(newIndex);

        }
        //now normals
        for (int i = 0; i < FINALVERTICES.size(); i++) {
            int newIndex = FINALVERTICES[i]->normal.x;
            positionData.push_back(newIndex);
        }
        for (int i = 0; i < FINALVERTICES.size(); i++) {
            int newIndex = FINALVERTICES[i]->normal.y;
            positionData.push_back(newIndex);
        }
//        for (int i = 0; i < FINALVERTICES.size(); i++) {
//            int newIndex = FINALVERTICES[i]->normal.z;
//            positionData.push_back(newIndex);
//        }
        //now textures
//        for (int i = 0; i < FINALVERTICES.size(); i++) {
//            int newIndex = FINALVERTICES[i]->texture.x;
//            positionData.push_back(newIndex);
//        }
//        for (int i = 0; i < FINALVERTICES.size(); i++) {
//            int newIndex = FINALVERTICES[i]->texture.y;
//            positionData.push_back(newIndex);
//        }



        const int IMAGE_RESOLUTION = 4096;
        int pCounter = 0;
        png::image< png::rgba_pixel > imager(IMAGE_RESOLUTION, IMAGE_RESOLUTION);
        for (size_t y = 0; y < imager.get_height(); ++y)
        {
            for (size_t x = 0; x < imager.get_width(); ++x)
            {
                if (pCounter < positionData.size()) {

                    if (pCounter < positionData.size()-1) {
                        unsigned int quantVal1 = (unsigned int) positionData[pCounter++];
                        unsigned int quantVal2= (unsigned int ) positionData[pCounter++];
                        //int r = (quantVal & 0x00FF0000) >> 16;
                        int r = (quantVal1 & 0x0000FF00) >> 8;
                        int g = (quantVal1 & 0x000000FF);
                        int b = (quantVal2 & 0x0000FF00) >> 8;
                        int a = (quantVal2 & 0x000000FF);

                        imager[y][x] = png::rgba_pixel(r, g, b, a);
                    }
                    else
                        imager[y][x] = png::rgba_pixel(0, 0, 0, 0);
                }
            }
        }
        imager.write("rgb.png");

        int iCounter = 0;
        png::image< png::rgb_pixel > imagei(IMAGE_RESOLUTION, IMAGE_RESOLUTION);
        for (size_t y = 0; y < imagei.get_height(); ++y)
        {
            for (size_t x = 0; x < imagei.get_width(); ++x)
            {
                if (iCounter < INDICES.size()) {

                    if (iCounter < INDICES.size()-1) {
//                        unsigned int r = (unsigned int )INDICES[iCounter];
//                        unsigned int g = (unsigned int )INDICES[iCounter+1];
//                        unsigned int b = (unsigned int )INDICES[iCounter+2];
//                        imagei[y][x] = png::rgb_pixel(r, g, b);
//                        iCounter+=3;
                        unsigned int quantVal = INDICES[iCounter++];
                        int r = (quantVal & 0x00FF0000) >> 16;
                        int g = (quantVal & 0x0000FF00) >> 8;
                        int b = (quantVal & 0x000000FF);
                        imagei[y][x] = png::rgb_pixel(r, g, b);
                    }
                    else
                        imagei[y][x] = png::rgb_pixel(0, 0, 0);
                }
                // non-checking equivalent of image.set_pixel(x, y, ...);
            }
        }

        imagei.write("index.png");




    }



    */
}

