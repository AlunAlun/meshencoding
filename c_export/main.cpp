#include <math.h>
#include <vector>
#include <iostream>
#include "glm/glm.hpp"
#include "Export.h"


#define degToRad(angleDegrees) (angleDegrees * M_PI / 180.0)
#define radToDeg(angleRadians) (angleRadians * 180.0 / M_PI)
const float PI = 3.1415927;


using namespace std;
using namespace glm;


int main(int argc, char *argv[]) {

    const string INFILEPATH = "/Users/alun/Dropbox/Work/Code/Projects/meshencoding/c_export/assets/buddha.obj";
    const string OUT_ROOT = "buddhatestonlyinds";

    Export::exportMesh(INFILEPATH, OUT_ROOT, 11, 0, MeshEncoding::UTF, IndexCoding::HIGHWATER, IndexCompression::PAIRED_TRIS);


    cout << "done!" << endl;
    return 0;

// Make sure that the output filename argument has been provided


}