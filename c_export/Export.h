//
// Created by Alun on 16/07/2015.
//

#ifndef C_EXPORT_EXPORT_H
#define C_EXPORT_EXPORT_H

#define GLM_SWIZZLE

#include <iostream>
#include <vector>
#include <map>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "glm/glm.hpp"
#include "glm/gtx/norm.hpp"
#include "Geometry.h"
#include "tiny_obj_loader.h"
//https://github.com/danielaparker/jsoncons
#include "jsoncons/json.hpp"

using jsoncons::json;
using jsoncons::json_exception;
using jsoncons::pretty_print;

using namespace std;
using namespace glm;

enum class MeshEncoding {UTF, PNG, VARINT};
enum class IndexCoding {DELTA, HIGHWATER};
enum class IndexCompression{NONE, PAIRED_TRIS};
enum class TriangleReordering{NONE, FORSYTH, TIPSIFY};
enum class VertexQuantization{GLOBAL, PER_AXIS};



typedef unsigned char uint8;
typedef unsigned short uint16;
typedef short int16;
typedef unsigned int uint32;

namespace Export {
    void exportMesh(const string INFILEPATH, const string OUT_ROOT, int bV, int bT,
                    MeshEncoding coding, IndexCoding indCoding, IndexCompression indCompression,
                    TriangleReordering triOrdering, VertexQuantization vertexQuantization);

}


#endif //C_EXPORT_EXPORT_H
