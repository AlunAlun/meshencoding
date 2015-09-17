//
// Created by Alun on 16/07/2015.
//

#include "Geometry.h"

namespace Geo {

    AABB* getAABB(vector<Vertex *> verts) {

        vec3 aabbMin(100000000.0, 100000000.0, 100000000.0);
        vec3 aabbMax(-100000000.0, -100000000.0, -100000000.0);
        for (int i = 0; i < verts.size(); i++) {
            vec3 cp = verts[i]->pos;
            if (cp.x < aabbMin.x) aabbMin.x = cp.x;
            if (cp.y < aabbMin.y) aabbMin.y = cp.y;
            if (cp.z < aabbMin.z) aabbMin.z = cp.z;
            if (cp.x > aabbMax.x) aabbMax.x = cp.x;
            if (cp.y > aabbMax.y) aabbMax.y = cp.y;
            if (cp.z > aabbMax.z) aabbMax.z = cp.z;
        }

        vec3 aabbRange(aabbMax.x - aabbMin.x, aabbMax.y - aabbMin.y, aabbMax.z - aabbMin.z);

        return new AABB(aabbMin, aabbRange);
    }

    vector<int> quantizeVertexPosition(vec3 pos, AABB *aabb, int bits) {
        float x = pos.x - aabb->min.x;
        float y = pos.y - aabb->min.y;
        float z = pos.z - aabb->min.z;

        x /= aabb->range.x;
        y /= aabb->range.y;
        z /= aabb->range.z;

        double quant = pow(2, bits) - 1;

        vector<int> qPos;
        qPos.push_back(int(x * quant));
        qPos.push_back(int(y * quant));
        qPos.push_back(int(z * quant));
        return qPos;
    }

    vector<int> quantizeVertexNormal(vec3 norm, int bits) {
        double quant = pow(2, bits) - 1;
        vector<int> qNorm;
        qNorm.push_back(int(((norm.x + 1) / 2) * quant));
        qNorm.push_back(int(((norm.y + 1) / 2) * quant));
        qNorm.push_back(int(((norm.z + 1) / 2) * quant));
        return qNorm;
    }

    // Returns ±1
    vec2 signNotZero(vec2 v) {
        return vec2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
    }
    // Assume normalized input. Output is on [-1, 1] for each component.
    vec2 float32x3_to_oct(vec3 v) {
        // Project the sphere onto the octahedron, and then onto the xy plane
        vec2 p = v.xy * (1.0f / (abs(v.x) + abs(v.y) + abs(v.z)));
        // Reflect the folds of the lower hemisphere over the diagonals
        return (v.z <= 0.0f) ? ((1.0f - abs(p.yx)) * signNotZero(p)) : p;
    }


    vec3 oct_to_float32x3(vec2 e) {
        vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
        if (v.z < 0) v.xy = (1.0f - abs(v.yx)) * signNotZero(v.xy);
        return normalize(v);
    }

    vector<int> quantizeVertexNormal_to_vec2(vec3 norm, int bits) {
        vec2 e = float32x3_to_oct(norm);

        double quant = pow(2, bits) - 1;
        vector<int> qNorm;
        qNorm.push_back(int(((e.x + 1) / 2) * quant));
        qNorm.push_back(int(((e.y + 1) / 2) * quant));
        if (qNorm[1] <= 255) {
            std::cout << norm.x << " " << norm.y << " " << norm.z <<"      ";
            std::cout << e.x << " " << e.y << "      ";
            std::cout << qNorm[0] << " " << qNorm[1] << std::endl;
        }
        return qNorm;
    }

    vector<int> quantizeVertexTexture(vec2 coord, int bits) {
        int quant = (int) pow(2, bits) - 1;
        if (coord.x > 1.0) coord.x = 1.0;
        if (coord.x < 0.0) coord.x = 0.0;
        if (coord.y > 1.0) coord.y = 1.0;
        if (coord.y < 0.0) coord.y = 0.0;

        vector<int> qCoord;
        qCoord.push_back(int(coord.x * quant));
        qCoord.push_back(int(coord.y * quant));
        return qCoord;
    }

    int getInterleavedUint(int val) {
        if (val <= 0)
            return -2 * val;
        else
            return ((val - 1) * 2) + 1;
    }

    float clamp(float x, float a, float b)
    {
        return x < a ? a : (x > b ? b : x);
    }

    float signNotZero(float value) {
        return value < 0.0 ? -1.0 : 1.0;
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

    //http://jcgt.org/published/0003/02/01/paper-lowres.pdf
    //https://cesiumjs.org/2015/05/18/Vertex-Compression/
    //https://github.com/AnalyticalGraphicsInc/cesium/blob/master/Source/Core/AttributeCompression.js#L43
    //https://github.com/AnalyticalGraphicsInc/cesium/blob/master/Source/Core/Math.js
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
}