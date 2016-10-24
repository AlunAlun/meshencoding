#include <math.h>
#include <vector>
#include <iostream>
#include <algorithm>
#include "glm/glm.hpp"
// #include "Export.h"
#include "webmesh.h"
#include "Geometry.h"


#define degToRad(angleDegrees) (angleDegrees * M_PI / 180.0)
#define radToDeg(angleRadians) (angleRadians * 180.0 / M_PI)



using namespace std;
// using namespace glm;

char* getCmdOption(char ** begin, char ** end, const std::string & option)
{
    char ** itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        return *itr;
    }
    return 0;
}

bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}

int main(int argc, char *argv[]) {

    printf("\nB128 WebMesh Generator (c) Alun Evans.\n Version 1.0.\n\n");

    if (argc < 3) {
        printf("Usage: ./webmesh options <input_file.obj> <outputfile>\n\n");
        printf("Options:\n");
        printf("-prog : creates a progressive mesh with default settings. All other options ignored.\n\n");
        printf("-b <value> : bits of precision for position, normals and textures. Default is 12\n\n");
        printf("-bV <value> : bits of quantisation for vertex positions. Default 12.\n\n");
        printf("-bN <value> : bits of quantisation for vertex normals (only used with normal quantisation). Default 12.\n\n");
        printf("-bT <value> : bits of quantisation for texture coordinates. Default 12.\n\n");
        printf("-co <delta|hwm> : Use either delta or high-watermark encoding for index buffer: Default hwm\n\n");
        printf("-tc <none|paired_tris> : Specify to use paired triangle index compression. Default paired_tris.\n\n");
        printf("-to <none|forsyth|tipsify> : Specify which triangle reordering algorithm. Default forsyth.\n\n");
        printf("-vq <global|per_axis>: Decide whether to adjust vertex quantisation precision based on axis length. Default per_axis.\n\n");
        printf("-n <quant|oct|fib>: Specify which method to use to encode normals. Default oct.\n\n");
        printf("-fib <value>. Number of normals to create in fibonacci sphere normals. Default 4096\n");
        printf("\n");
        return 0;
    }

    const string INFILEPATH = argv[argc-2];
    const string OUT_ROOT = argv[argc-1];

    
    bool progressive= cmdOptionExists(argv, argv + argc, "-prog");
    
    if (!progressive) {
        WebMesh wm(INFILEPATH, OUT_ROOT);
        
        char *op = getCmdOption(argv, argv + argc, "-b");
        if (op) wm.setQuantizationBitDepth(atoi(op), atoi(op), atoi(op));

        op = getCmdOption(argv, argv + argc, "-bV");
        if (op) wm.setQuantizationBitDepthPosition(atoi(op));

        op = getCmdOption(argv, argv + argc, "-bN");
        if (op) wm.setQuantizationBitDepthNormal(atoi(op));

        op = getCmdOption(argv, argv + argc, "-bT");
        if (op) wm.setQuantizationBitDepthTexture(atoi(op));

        op = getCmdOption(argv, argv + argc, "-co");
        if (op) {
            if (!strcmp(op, "delta")) wm.setIndexCoding(IndexCoding::DELTA);
            else if (!strcmp(op,"hwm")) wm.setIndexCoding(IndexCoding::HIGHWATER);
        }

        op = getCmdOption(argv, argv + argc, "-tc");
        if (op) {
            if (!strcmp(op,"none")) wm.setIndexCompression(IndexCompression::NONE);
            else if (!strcmp(op, "paired_tris")) wm.setIndexCompression(IndexCompression::PAIRED_TRIS);
        }

        op = getCmdOption(argv, argv + argc, "-to");
        if (op) {
            cout << op << endl;
            if (!strcmp(op,"none")) {wm.setTriangleReordering(TriangleReordering::NONE);printf("lol");}
            else if (!strcmp(op,"forsyth")) wm.setTriangleReordering(TriangleReordering::FORSYTH);
            else if (!strcmp(op,"tipsify")) wm.setTriangleReordering(TriangleReordering::TIPSIFY);
        }

        op = getCmdOption(argv, argv + argc, "-vq");
        if (op) {
            if (!strcmp(op,"global")) wm.setVertexQuantization(VertexQuantization::GLOBAL);
            else if (!strcmp(op , "per_axis")) wm.setVertexQuantization(VertexQuantization::PER_AXIS);
        }

        op = getCmdOption(argv, argv + argc, "-n");
        if (op) {
            if (!strcmp(op,"quant")) wm.setNormalEncoding(NormalEncoding::QUANT);
            else if (!strcmp(op,"oct")) wm.setNormalEncoding(NormalEncoding::OCT);
            else if (!strcmp(op,"fib")) wm.setNormalEncoding(NormalEncoding::FIB);
        }

        op = getCmdOption(argv, argv + argc, "-fib");
        if (op) {
            wm.setFibLevels(atoi(op));
        }

        wm.exportMesh(true); 

    } 
    else { // progressive with defaults
        printf("Creating progressive mesh with defaults.\n\n");

        WebMesh webMesh1(INFILEPATH, OUT_ROOT+"_a");
        webMesh1.setQuantizationBitDepth(7,7,8);
        webMesh1.setNormalEncoding(NormalEncoding::FIB);
        webMesh1.setFibLevels(256);
        json metadata1 = webMesh1.exportMesh(false); 

        WebMesh webMesh2(INFILEPATH, OUT_ROOT+"_b");
        webMesh2.setQuantizationBitDepth(8,8,11);
        webMesh2.setNormalEncoding(NormalEncoding::FIB);
        webMesh2.setFibLevels(256);
        json metadata2 = webMesh2.exportMesh(false); 

        WebMesh webMesh3(INFILEPATH, OUT_ROOT+"_c");
        webMesh3.setQuantizationBitDepth(11,11,11);
        webMesh3.setNormalEncoding(NormalEncoding::OCT);
        webMesh3.setFibLevels(256);
        json metadata3 = webMesh3.exportMesh(false); 


        json LODS(json::an_array);
        LODS.add(metadata1["meshes"]);
        LODS.add(metadata2["meshes"]);
        LODS.add(metadata3["meshes"]);

        json mats = metadata1["materials"];

        json rootJSON;
        rootJSON["progressive"] = LODS;
        rootJSON["materials"] = mats;
        ofstream jsonFile;
        jsonFile.open (OUT_ROOT + ".json");
        jsonFile << rootJSON;
        jsonFile.close();

        //now concatenate files
        // std::ifstream if_a(OUT_ROOT+"_a.b128", std::ios_base::binary);
        // std::ifstream if_b(OUT_ROOT+"_b.b128", std::ios_base::binary);
        // std::ifstream if_c(OUT_ROOT+"_c.b128", std::ios_base::binary);
        // std::ofstream of_d(OUT_ROOT+".b128", std::ios_base::binary);

        // of_d << if_a.rdbuf() << if_b.rdbuf() << if_c.rdbuf();
    }
    
    cout << "done!" << endl;
    return 0;
}
