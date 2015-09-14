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
            cout << "invalid code_point " << code_point << endl;
        }
    }

    void pushSafeInterleavedUTF(int delta, int interleaved, vector<int>& indices) {
        if (interleaved >= 55295 && interleaved <= 57343) {
            if (delta < 0)
                delta = delta + 14000;
            else
                delta = delta - 14000;
            indices.push_back(55295);
            indices.push_back(Geo::getInterleavedUint(delta));
        }
        else
            indices.push_back(interleaved);
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


//    vec2 signNotZero(vec2 v) {
//        return vec2((v.x >= 0.0f) ? +1.0f : -1.0f, (v.y >= 0.0f) ? +1.0f : -1.0f);
//    }

    float signNotZero(float value) {
        return value < 0.0 ? -1.0 : 1.0;
    }

    inline float clamp(float x, float a, float b)
    {
        return x < a ? a : (x > b ? b : x);
    }

    int toSNorm(float value) {
        return (int)round((clamp(value, -1.0, 1.0) * 0.5 + 0.5 ) * 255.0);
    }

    float fromSNorm(int value){
        return clamp(value, 0.0, 255.0) / 255.0 * 2.0 - 1.0;
    }

    // Assume normalized input. Output is on [0, 255] for each component,
    // representing -1.0 to +1.0;
    vec2 octEncode_8bit(vec3 v) {
        vec2 result = vec2(v.x / (abs(v.x) + abs(v.y) + abs(v.z)),
                           v.y / (abs(v.x) + abs(v.y) + abs(v.z)));
        if (v.z < 0) {
            float x = result.x;
            float y = result.y;
            result = vec2( (1.0 - abs(y)) * signNotZero(x),
                           (1.0 - abs(x)) * signNotZero(y) );

        }
        result = vec2(toSNorm(result.x), toSNorm(result.y));

        return result;
    }

    vec3 octDecode_8bit(vec2 v) {
        float rX = fromSNorm(v.x);
        float rY = fromSNorm(v.y);
        float rZ = 1.0 - (abs(rX) + abs(rY));

        if (rZ < 0.0) {
            float oldVX = rX;
            rX = (1.0 - abs(rY)) * signNotZero(oldVX);
            rY = (1.0 - abs(oldVX)) * signNotZero(rY);
        }

        return normalize(vec3(rX, rY, rZ));
    }



    void exportUTFMesh(const string INFILEPATH, const string OUT_ROOT, int bV, int bN, int bT) {

        //http://jcgt.org/published/0003/02/01/paper-lowres.pdf
        //https://cesiumjs.org/2015/05/18/Vertex-Compression/
        //https://github.com/AnalyticalGraphicsInc/cesium/blob/master/Source/Core/AttributeCompression.js#L43
        //https://github.com/AnalyticalGraphicsInc/cesium/blob/master/Source/Core/Math.js

        vector<Vertex*> ORIGVERTICES;
        map<Vertex, int> VERTICES_MAP;
        vector<Vertex*> VERTICES;
        vector<Face*> FACES;

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
        for (int i = 0; i < m.positions.size()/3; i++) {
            Vertex *newVertex = new Vertex(vec3(m.positions[i*3],m.positions[i*3+1],m.positions[i*3+2]));
            vec3 n = vec3(m.normals[i*3],m.normals[i*3+1],m.normals[i*3+2]);
            if (abs(n.x) == 0.0 && abs(n.y) == 0.0 && abs(n.z) == 0.0 ) {
                n = vec3(0.0001, 0.0001, 0.0001);
                printf("replace\n");
            }

             newVertex->normal = glm::normalize(n);

            if (m.texcoords.size() != 0)
                newVertex->texture = vec2(m.texcoords[i*2],m.texcoords[i*2+1]);
            ORIGVERTICES.push_back(newVertex);
        }


        //get AABB
        AABB* aabb = Geo::getAABB(ORIGVERTICES);
        //quantize everything
        for (int i = 0; i < ORIGVERTICES.size(); i++){
            Vertex * currVert = ORIGVERTICES[i];
            vector<int> currPos = (vector<int>)Geo::quantizeVertexPosition(currVert->pos, aabb, bV);

            Vertex *newVertex = new Vertex(vec3(currPos[0], currPos[1], currPos[2]));
            newVertex->normal2 = octEncode_8bit(currVert->normal);

            vector<int> currCoords = (vector<int>)Geo::quantizeVertexTexture(currVert->texture, bT);
            newVertex->texture = vec2(currCoords[0],currCoords[1]);
            VERTICES.push_back(newVertex);

        }



        cout << "Reindexing..." << endl;

        //reindex
        int newVertCounter = 0;
        std::map<Vertex, int>::iterator it;

        //for each set of three indices
        for (int i = 0; i < m.indices.size(); i+=3) {
            //creat empty vector for new face indices
            vector<int> newFaceIndices;
            //loop through next three indices
            for (int j = 0; j < 3; j++) {
                //get current index
                int index = m.indices[i+j];
                //get vert referenced by pointer
                Vertex vert = *VERTICES[index];
                //try to find vertex in map
                it = VERTICES_MAP.find(vert);
                //if its there, add the index to the newFaceindex list
                if (it != VERTICES_MAP.end()) {
                    newFaceIndices.push_back(it->second);
                }
                    //otherwise, add size-1 of current map as index
                    //and add vert to map
                else {
                    newFaceIndices.push_back(newVertCounter);
                    VERTICES_MAP[vert] = newVertCounter;
                    newVertCounter++; // now increment
                }
            }

            Face *newFace = new Face(newFaceIndices[0], newFaceIndices[1], newFaceIndices[2]);
            if (newFaceIndices[0] < 0)
                cout << newFaceIndices[0] << " " << newFaceIndices[1] << " " << newFaceIndices[2] << endl;
            FACES.push_back(newFace);


        }

        //now flip the map
        map<int, Vertex*> FLIPMAP;

        for (auto itr = VERTICES_MAP.begin(); itr != VERTICES_MAP.end(); itr++) {
            FLIPMAP[itr->second] = (Vertex*)&(itr->first);
        }

        cout << VERTICES.size() << endl;
        cout << FLIPMAP.size() << endl;

        cout << "Compressing..." << endl;

        //quantize and delta encode vertex positions
        vector<int> VTXPosX;
        vector<int> VTXPosY;
        vector<int> VTXPosZ;
        vector<int> VTXNormX;
        vector<int> VTXNormY;
        vector<int> VTXCoordU;
        vector<int> VTXCoordV;
        int lastPosX = 0, lastPosY = 0, lastPosZ = 0,
                lastNormX = 0, lastNormY = 0, lastNormZ = 0,
                lastCoordU = 0, lastCoordV = 0, delta = 0;
        for (int i = 0; i < FLIPMAP.size(); i++) {

            Vertex * currVert = FLIPMAP[i];

            vec3 currPos = currVert->pos;
            vec2 currNorm = currVert->normal2;
            vec2 currCoords = currVert->texture;


            delta = (int)currPos.x - (int)lastPosX;
            lastPosX = (int)currPos.x;
            VTXPosX.push_back(Geo::getInterleavedUint(delta));

            delta = (int)currPos.y - (int)lastPosY;
            lastPosY = (int)currPos.y;
            VTXPosY.push_back(Geo::getInterleavedUint(delta));

            delta = (int)currPos.z - (int)lastPosZ;
            lastPosZ = (int)currPos.z;
            VTXPosZ.push_back(Geo::getInterleavedUint(delta));

            delta = (int)currNorm.x - (int)lastNormX;
            lastNormX = (int)currNorm.x;
            VTXNormX.push_back(Geo::getInterleavedUint(delta));

            delta = (int)currNorm.y - (int)lastNormY;
            lastNormY = (int)currNorm.y;
            VTXNormY.push_back(Geo::getInterleavedUint(delta));

            if (bT > 0) {
                delta = (int)currCoords.x - (int)lastCoordU;
                lastCoordU = (int)currCoords.x;
                VTXCoordU.push_back(Geo::getInterleavedUint(delta));

                delta = (int)currCoords.y - (int)lastCoordV;
                lastCoordV = (int)currCoords.y;
                VTXCoordV.push_back(Geo::getInterleavedUint(delta));
            }
        }

        //delta encode indices
        vector<int> indices;
        int lastIndex = 0, index = 0, interleaved = 0;
        delta = 0;
        int fourByters = 0;
        int nextHighWaterMark = 0;
        int toStore = 0;
        for (int i = 0; i < FACES.size(); i++) {
            for (int j = 0; j < FACES[i]->indices.size(); j++) {

                index = FACES[i]->indices[j];
                toStore = (nextHighWaterMark-index);
                if (toStore > 65535) //count the number of indices which need more than 16-bit to store
                    fourByters++;


                pushSafeUTF(toStore, indices);

                if (index == nextHighWaterMark)
                    nextHighWaterMark++;



            }

        }
        cout << fourByters << endl;
        cout << indices.size() << endl;



        //write files
        cout << "Writing files..." << endl;

        //first write utfFile (in order to get it's size for JSON
        FILE * utfFile;
        string outUTF8Name = OUT_ROOT + ".utf8";
        utfFile = fopen (outUTF8Name.c_str(),"w");
        if (utfFile!=NULL) {
            for (int i = 0; i < VTXPosX.size(); i++) {
                write_utf8(VTXPosX[i], utfFile);
            }
            for (int i = 0; i < VTXPosY.size(); i++) {
                write_utf8(VTXPosY[i], utfFile);
            }
            for (int i = 0; i < VTXPosZ.size(); i++) {
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
            for (int i = 0; i < indices.size(); i++) {
                write_utf8(indices[i], utfFile);
            }
            fclose (utfFile);


        }


        struct stat filestatus;
        stat( outUTF8Name.c_str(), &filestatus );
        cout << int(filestatus.st_size)/1000 << " bytes\n";


        json metadata;
        json lodArray(json::an_array);

        json currLod;
        currLod["vertices"] = int(FLIPMAP.size());
        currLod["faces"] = int(FACES.size());
        currLod["utfIndexBuffer"] = int(indices.size())+fourByters; //add number of 3 or 4 byte indices for utf parsing

        json currBits;
        currBits["vertex"] = int(bV);
        currBits["normal"] = int(bN);
        currBits["texture"] = int(bT);
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
}