//
// Created by Alun on 5/11/2015.
//

#ifndef WEBMESH_H
#define WEBMESH_H

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
enum class NormalEncoding{QUANT, OCT, FIB};

class WebMesh {
public: 
	WebMesh(string infile, string out_root);
	json exportMesh(bool writeHFile = true);
	void setQuantizationBitDepth(int aBV, int aBN, int aBT);
	void setQuantizationBitDepthPosition(int aBV);
	void setQuantizationBitDepthNormal(int aBN);
	void setQuantizationBitDepthTexture(int aBT);
	void setFibLevels(int aFibLevels);
	void setOutputFormat(MeshEncoding output);
	void setIndexCoding(IndexCoding ic);
	void setIndexCompression(IndexCompression ic);
	void setTriangleReordering(TriangleReordering t);
	void setVertexQuantization(VertexQuantization v);
	void setNormalEncoding(NormalEncoding n);

private:
	//these variables are used to set the coding of the current webmesh
	string m_INFILE;
	string m_OUT_ROOT;
	string m_OUT_FILENAME;
	int m_bV;
	int m_bN;
	int m_bT;
	int m_fibLevels;
	MeshEncoding m_meshEncoding;
	IndexCoding m_indCoding;
	IndexCompression m_indCompression;
	TriangleReordering m_triOrdering;
	VertexQuantization m_vertexQuantization;
	NormalEncoding m_normalEncoding;
	
	//private variables
	bool m_writeHeaderFile;
	vector<tinyobj::shape_t> m_shapes;
	vector<tinyobj::material_t> m_materials;

	//private functions
	void readOBJ(const string INFILEPATH);
	json processOneShape(tinyobj::shape_t currShape, 
                        vector<int>& vertices_to_write, 
                        vector<int>& indices_to_write);
	json writeHeader(vector<int>& vertices_to_write,
	                vector<int>& indices_to_write,
	                AABB* aabb,
	                int numVerts,
	                int numFaces,
	                int fourByters,
	                int max_hw_step,
	                int mat_id,
	                const char* name,
	                int needReconstructNormals,
	                int bVx = 11,
	                int bVy = 11,
	                int bVz = 11);

};


#endif //WEBMESH_H