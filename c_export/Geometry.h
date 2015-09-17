//
// Created by Alun on 16/07/2015.
//

#ifndef C_EXPORT_GEOMETRY_H
#define C_EXPORT_GEOMETRY_H
#define GLM_SWIZZLE

#include <iostream>
#include <vector>
#include "glm/glm.hpp"


using namespace std;
using namespace glm;

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
    vec3 normal;
    vec2 normal2;
    vec3 color;
    vec2 texture;
    Vertex(vec3 _pos) {
        pos = _pos;
        color = vec3(1.0,1.0,1.0);
        texture = vec2(0.0,0.0);

    }
    bool operator < (const Vertex &b)const {
        return tie(pos.x, pos.y, pos.z) < tie(b.pos.x, b.pos.y, b.pos.z);
    }
    bool operator == (const Vertex& b)const{
        return pos.x == b.pos.x && pos.y == b.pos.y && pos.z == b.pos.z ? true : false;
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

namespace Geo {
    AABB* getAABB(vector<Vertex*> verts);
    vector<int> quantizeVertexPosition(vec3 pos, AABB* aabb, int bits);
    vector<int> quantizeVertexNormal(vec3 norm, int bits);
    vector<int> quantizeVertexNormal_to_vec2(vec3 norm, int bits);
    vector<int> quantizeVertexTexture(vec2 coord, int bits);
    int getInterleavedUint(int val);
    vec2 octEncode_8bit(vec3 v);
    vec3 octDecode_8bit(vec2 v);
    float clamp(float x, float a, float b);
    float signNotZero(float value);
    int toSNorm(float value);
    float fromSNorm(int value);
}

#endif //C_EXPORT_GEOMETRY_H
