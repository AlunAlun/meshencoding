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

    const string INFILEPATH = "/Users/alun/Dropbox/Work/Code/Projects/meshencoding/c_export/assets/bunny.obj";
    const string OUT_ROOT = "bunny";

    Export::exportUTFMesh(INFILEPATH, OUT_ROOT, 11, 8, 0);
    //Export::exportPNGMesh(INFILEPATH, OUT_ROOT);

    cout << "done!" << endl;
    return 0;

// Make sure that the output filename argument has been provided


}