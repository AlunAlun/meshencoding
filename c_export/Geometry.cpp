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

        double quant = pow(2, bits);

        vector<int> qPos;
        qPos.push_back(round(x * quant));
        qPos.push_back(round(y * quant));
        qPos.push_back(round(z * quant));
        return qPos;
    }
    vector<int> quantizeVertexPosition(vec3 pos, AABB *aabb, int bVx, int bVy, int bVz) {
        float x = pos.x - aabb->min.x;
        float y = pos.y - aabb->min.y;
        float z = pos.z - aabb->min.z;

        x /= aabb->range.x;
        y /= aabb->range.y;
        z /= aabb->range.z;

        double quantX = pow(2, bVx);
        double quantY = pow(2, bVy);
        double quantZ = pow(2, bVz);

        vector<int> qPos;
        qPos.push_back(round(x * quantX));
        qPos.push_back(round(y * quantY));
        qPos.push_back(round(z * quantZ));
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
        vector<int> qCoord;
        
        int quant = (int) pow(2, bits);
        qCoord.push_back(int(coord.x * quant));
        qCoord.push_back(int(coord.y * quant));
        return qCoord;
    }

    vector<vec3> computeFibonacci_sphere(int samples) {
        int rnd = 1.0;
        vector<vec3> points;
        float offset = 2.0/(float)samples;
        float increment = PI * (3.0 - sqrt(5.0));

        for (int i = 0; i < samples; i++){
            float y = ((i * offset) - 1) + (offset / 2);
            float r = sqrt(1 - pow(y, 2));

            float phi = (float)((i + rnd) % samples) * increment;

            float x = cos(phi) * r;
            float z = sin(phi) * r;

            points.push_back(vec3(x,y,z));
        }
        return points;
    }

    int computeFibonacci_normal(vec3 theNorm, vector<vec3> &fibSphere) {
        vec3 norm = glm::normalize(theNorm);
        //get dot products of all fib vectors with norm
        float maxDP = -1.0;
        int maxIndex = -1;
        float dp;
        for (size_t i = 0; i < fibSphere.size(); i++) {
            dp = glm::dot(norm, fibSphere[i]);
            if (dp > maxDP){
                maxDP = dp;
                maxIndex = i;
            }
        }
        // printf("%.2f %.2f %.2f : %.2f %.2f %.2f\n", theNorm.x, theNorm.y, theNorm.z,
        //                         fibSphere[maxIndex].x, fibSphere[maxIndex].y, fibSphere[maxIndex].z);
        //now minIndex is the index in the fibonacci sphere which contains the nearest normal
        return maxIndex;
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

    //does not remotely work
    void compressIndexBufferFIFO(vector<int>& inds_in, vector<int>&inds_out) {
        int CACHE_SIZE = 32;
        uint32 cache[CACHE_SIZE];
        for (size_t i = 0; i < inds_in.size(); i++) {
            uint32 currIndex = inds_in[i];

            //search cache
            int found = -1;
            for (size_t j = 0; j < CACHE_SIZE; j++) {
                if (cache[j] == currIndex)
                    found = j;
            }
            if (found >= 0) {
                inds_out.push_back(found); // push index in cache
            }
            else {

                inds_out.push_back(currIndex);
                //update cache
                for (size_t j = 0; j < CACHE_SIZE-1; j++) {
                    cache[j+1] = cache[j];
                }
                cache[0] = currIndex;
            }
        }
    }



}