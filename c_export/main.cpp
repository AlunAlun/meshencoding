#include <math.h>
#include <vector>
#include <iostream>
#include "glm/glm.hpp"
#include "Export.h"
#include "Geometry.h"


#define degToRad(angleDegrees) (angleDegrees * M_PI / 180.0)
#define radToDeg(angleRadians) (angleRadians * 180.0 / M_PI)



using namespace std;
using namespace glm;


int main(int argc, char *argv[]) {

    const string INFILEPATH = "/Users/alun/Dropbox/Work/Code/Projects/meshencoding/c_export/assets/buddha.obj";
    const string OUT_ROOT = "buddha";

    Export::exportMesh(INFILEPATH, OUT_ROOT, 11, 0,
                       MeshEncoding::VARINT,
                       IndexCoding::HIGHWATER,
                       IndexCompression::PAIRED_TRIS,
                       TriangleReordering::FORSYTH,
                       VertexQuantization::PER_AXIS);



    cout << "done!" << endl;
    return 0;
}


/*

#ifdef putc_unlocked
# define PutChar putc_unlocked
#else
# define PutChar putc
#endif  // putc_unlocked

const char *byte_to_binary(unsigned x)
{
    static char b[9];
    b[0] = '\0';

    int z;
    for (z = 128; z > 0; z >>= 1)
    {
        strcat(b, ((x & z) == z) ? "1" : "0");
    }

    return b;
}

char * int2bin(int i)
{
    size_t bits = sizeof(int) * CHAR_BIT;

    char * str = (char*) malloc(bits + 1);
    if(!str) return NULL;
    str[bits] = 0;

    // type punning because signed shift is implementation-defined
    unsigned u = *(unsigned *)&i;
    for(; bits--; u >>= 1)
        str[bits] = u & 1 ? '1' : '0';

    return str;
}


void write_varint(unsigned value, FILE* file) {
    if (value < 0x80) {
        PutChar(static_cast<char>(value), file);

    } else if( value < 0x4000) {

        unsigned char firstByte = (static_cast<char>(value) & 0x7F); // first seven bits
        firstByte |= 1 << 7;
        PutChar(static_cast<char>(firstByte), file);

        unsigned shiftVal = value >> 7; //shift off first 7 bits
        char secondByte = (static_cast<char>(shiftVal) & 0x7F); // next seven bits
        PutChar(static_cast<char>(secondByte), file);


    } else if (value < 0x200000) {
        unsigned char firstByte = (static_cast<char>(value) & 0x7F); // first seven bits
        firstByte |= 1 << 7;
        PutChar(static_cast<char>(firstByte), file);

        unsigned shiftVal = value >> 7; //shift off first 7 bits
        unsigned char secondByte = (static_cast<char>(shiftVal) & 0x7F); // next seven bits
        secondByte |= 1 << 7;
        PutChar(static_cast<char>(secondByte), file);

        shiftVal = value >> 14; //shift off first 14 bits
        unsigned char thirdByte = (static_cast<char>(shiftVal) & 0x7F); // next seven bits
        PutChar(static_cast<char>(thirdByte), file);
    }
}

char *readFile(char *fileName) {
    FILE *file = fopen(fileName, "r");
    char *code;
    size_t n = 0;
    int c;

    if (file == NULL) return NULL; //could not open file
    fseek(file, 0, SEEK_END);
    long f_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    code = (char*)malloc(f_size);

    while ((c = fgetc(file)) != EOF) {
        code[n++] = (char)c;
    }

    code[n] = '\0';

    return code;
}

*/



//    FILE * varintFile;
//
//    varintFile = fopen ("test.vint","w");
//    if (varintFile!=NULL) {
//        int theVal = 67904;
//        write_varint(theVal, varintFile);
//        printf("tv %s\n", int2bin(theVal));
//        fclose(varintFile);
//    }
//
//    char* inBuffer = readFile("test.vint");
//    unsigned char c1, c2, c3, c4, c5;
//    uint32 i1, i2, i3, i4, i5, result;
//    size_t n = 0;
//    size_t counter = 0;
//
//    while ( (c1 = inBuffer[n++]) != '\0') {
//
//        if ( (c1 & 0x80) ) { // first bit is set, so we need to read another byte
//            c2 = inBuffer[n++]; //get next byte
//            if (c2 & 0x80) { // first bit is set, so we need to read a third byte
//                c3 = inBuffer[n++];
//
//                printf("c1 %s\n", byte_to_binary(c1));
//                printf("c2 %s\n", byte_to_binary(c2));
//                printf("c3 %s\n", byte_to_binary(c3));
//
//
//                i1 = c1; //cast first byte to uint32
//                printf("i1 %s\n", int2bin(i1));
//                i1 &= ~(1 << 7); // set marker bit of i1 to zero
//                printf("1m %s\n", int2bin(i1));
//
//                i2 = c2 << 7; //cast second byte to uint32
//                printf("i2 %s\n", int2bin(i2));
//                i2 &= ~(1 << 14); // set marker bit of i2 to zero
//                printf("2m %s\n", int2bin(i2));
//
//                printf("i3 %s\n", int2bin(i3));
//                i3 = c3 << 14; //cast third byte to uint32
//                printf("3m %s\n", int2bin(i3));
//
//                result = i1 | i2 | i3;
//                printf("result is: %d\n", result);
//            }
//            else { // process the twy bytes
//                i1 = c1; //cast first byte to uint32
//                i1 &= ~(1 << 7); // set marker bit of i1 to zero
//                i2 = c2 << 7; //cast second byte to uint32
//                result = i1 | i2; // join both chars
//                printf("result is: %d\n", result);
//            }
//
//
//
//        }
//
//    }
