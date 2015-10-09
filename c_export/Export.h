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

enum class MeshEncoding {UTF, PNG};
enum class IndexCoding {DELTA, HIGHWATER};
enum class IndexCompression{NONE, PAIRED_TRIS};

typedef unsigned int u_int_32;

namespace Export {
    void exportMesh(const string INFILEPATH, const string OUT_ROOT, int bV, int bT,
                    MeshEncoding coding, IndexCoding indCoding, IndexCompression indCompression);

}


#endif //C_EXPORT_EXPORT_H
