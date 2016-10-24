//
// Created by Alun on 16/07/2015.
//

#ifndef C_EXPORT_GEOMETRY_H
#define C_EXPORT_GEOMETRY_H
#define GLM_SWIZZLE

#include <iostream>
#include <vector>
#include <queue>
#include <math.h>
#include "glm/glm.hpp"


using namespace std;
using namespace glm;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef short int16;
typedef unsigned int uint32;

const float PI = 3.1415927;

struct Face{
    int facenum;
    bool visited;
    vector<int> indices;
    vec3 normal;
    float area;
    vec3 centroid;
    Face(int f1,int f2,int f3) {  //constructor for triangle
        indices.push_back(f1);
        indices.push_back(f2);
        indices.push_back(f3);
        visited = false;
    }
};

struct Vertex {
    vec3 pos;
    vec3 normal; //standard 3 channel normal
    vec2 normal2; //octahedral normal
    int normal1; //index into fibonacci sphere
    vec3 color;
    vec2 texture;
    Vertex(vec3 _pos) {
        pos = _pos;
        color = vec3(1.0,1.0,1.0);
        texture = vec2(0.0,0.0);

    }
    bool operator < (const Vertex &b)const {
        return tie(pos.x, pos.y, pos.z, texture.x, texture.y) < tie(b.pos.x, b.pos.y, b.pos.z, b.texture.x, b.texture.y);
    }
    bool operator == (const Vertex& b)const{
        return pos.x == b.pos.x && pos.y == b.pos.y && pos.z == b.pos.z && texture.x == b.texture.x && texture.y == b.texture.y ? true : false;
    }
};

struct AABB {
    vec3 min;
    vec3 range;
    AABB(vec3 _min, vec3 _range) {
        min = _min;
        range = _range;
    }
};

enum IndexBufferTriangleCodes
{
    IB_INDEX_NEW = 0,
    IB_INDEX_CACHED = 1
};

namespace Geo {
    AABB* getAABB(vector<Vertex*> verts);
    vector<int> quantizeVertexPosition(vec3 pos, AABB* aabb, int bits);
    vector<int> quantizeVertexPosition(vec3 pos, AABB* aabb, int bVx, int bVy, int bVz);
    vector<int> quantizeVertexNormal(vec3 norm, int bits);
    vector<int> quantizeVertexNormal_to_vec2(vec3 norm, int bits);
    vector<int> quantizeVertexTexture(vec2 coord, int bits);
    vector<vec3> computeFibonacci_sphere(int samples);
    int computeFibonacci_normal(vec3 norm, vector<vec3> &fibSphere);
    int getInterleavedUint(int val);
    vec2 octEncode_8bit(vec3 v);
    vec3 octDecode_8bit(vec2 v);
    float clamp(float x, float a, float b);
    float signNotZero(float value);
    int toSNorm(float value);
    float fromSNorm(int value);
    void compressIndexBuffer(vector<int>& inds_in, vector<int>&inds_out);
    void compressIndexBufferFIFO(vector<int>& inds_in, vector<int>&inds_out);
}

#endif //C_EXPORT_GEOMETRY_H
