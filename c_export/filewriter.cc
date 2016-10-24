#include "filewriter.h"

#include <png++/png.hpp>
#include <sys/types.h>
#include <sys/stat.h>

namespace filewriter {

int write_varint(unsigned value, FILE* file) {
    if (value < 0x80) {
        PutChar(static_cast<char>(value), file);
        return 1;

    } else if( value < 0x4000) {

        unsigned char firstByte = (static_cast<char>(value) & 0x7F); // first seven bits
        firstByte |= 1 << 7;
        PutChar(static_cast<char>(firstByte), file);

        unsigned shiftVal = value >> 7; //shift off first 7 bits
        char secondByte = (static_cast<char>(shiftVal) & 0x7F); // next seven bits
        PutChar(static_cast<char>(secondByte), file);
        return 2;

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
        return 3;
    } else if (value < 0x10000000){
        cout << "invalid b128 value " << value << endl;
        return 0;
    }
    return 0;
}

void write_utf8(unsigned code_point, FILE * utfFile)
{
    if (code_point < 0x80) {
        PutChar(static_cast<char>(code_point), utfFile);
    } else if (code_point <= 0x7FF) {
        PutChar(static_cast<char>((code_point >> 6) + 0xC0), utfFile);
        PutChar(static_cast<char>((code_point & 0x3F) + 0x80), utfFile);
    } else if (code_point <= 0xFFFF) {
        PutChar(static_cast<char>((code_point >> 12) + 0xE0), utfFile);
        PutChar(static_cast<char>(((code_point >> 6) & 0x3F) + 0x80), utfFile);
        PutChar(static_cast<char>((code_point & 0x3F) + 0x80), utfFile);
    } else if (code_point <= 0x10FFFF) {
        PutChar(static_cast<char>((code_point >> 18) + 0xF0), utfFile);
        PutChar(static_cast<char>(((code_point >> 12) & 0x3F) + 0x80), utfFile);
        PutChar(static_cast<char>(((code_point >> 6) & 0x3F) + 0x80), utfFile);
        PutChar(static_cast<char>((code_point & 0x3F) + 0x80), utfFile);
    } else {

        cout << "invalid utf8 code_point " << code_point << endl;
    }
}

int writeDataVARINT(std::vector<int>& buffer_to_write, const string OUT_ROOT) {

    //write files
    std::cout << "Writing varint data..." << std::endl;

    //first write utfFile (in order to get it's size for JSON
    FILE * vintFile;
    std::string outVINTName = OUT_ROOT + ".b128";
    vintFile = fopen (outVINTName.c_str(),"w");
    int numBytes = 0;
    if (vintFile!=NULL) {
        for (int i = 0; i < buffer_to_write.size(); i++) {
            numBytes+=write_varint(buffer_to_write[i], vintFile);
        }
        fclose (vintFile);
    }

    struct stat filestatus;
    stat( outVINTName.c_str(), &filestatus );
    std::cout << int(filestatus.st_size)/1000 << " bytes\n";
    return numBytes;
}

//is Broken!!
void writeDataUTF(vector<int>& vertices_to_write,
                  vector<int>& indices_to_write,
                  const string OUT_ROOT) {

    //write files
    cout << "Writing data..." << endl;

    //first write utfFile (in order to get it's size for JSON
    FILE * utfFile;
    string outUTF8Name = OUT_ROOT + ".utf8";
    utfFile = fopen (outUTF8Name.c_str(),"w");
    if (utfFile!=NULL) {
        int counter = 0;
        for (int i = 0; i < vertices_to_write.size(); i++) {

            write_utf8(vertices_to_write[i], utfFile);
            counter++;
        }

        for (int i = 0; i < indices_to_write.size(); i++) {
            if (counter > 319560 && counter < 319575)
                printf("%d ", indices_to_write[i]);
            write_utf8(indices_to_write[i], utfFile);
            counter++;
        }

        fclose (utfFile);


    }

    struct stat filestatus;
    stat( outUTF8Name.c_str(), &filestatus );
    cout << int(filestatus.st_size)/1000 << " bytes\n";
}


void writeDataPNG(vector<int>& buffer_to_write,
                const string OUT_ROOT) {

    const int IMAGE_RESOLUTION = 4096;

    png::image< png::rgb_pixel > imagei(IMAGE_RESOLUTION, IMAGE_RESOLUTION);

    int iCounter = 0;

    for (size_t y = 0; y < imagei.get_height(); ++y) {
        for (size_t x = 0; x < imagei.get_width(); ++x) {

            if (iCounter < buffer_to_write.size()){
                unsigned int quantVal = buffer_to_write[iCounter++];

                int r = (quantVal & 0x00FF0000) >> 16;
                int g = (quantVal & 0x0000FF00) >> 8;
                int b = (quantVal & 0x000000FF);
                imagei[y][x] = png::rgb_pixel(r, g, b);

            }
        }
    }
    imagei.write(OUT_ROOT + ".png");

}

}