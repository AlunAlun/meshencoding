#ifndef FILE_WRITER_H
#define FILE_WRITER_H

#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <vector>

using namespace std;

#ifdef putc_unlocked
# define PutChar putc_unlocked
#else
# define PutChar putc
#endif  // putc_unlocked

namespace filewriter {

int write_varint(unsigned value, FILE* file);

void write_utf8(unsigned code_point, FILE * utfFile);

int writeDataVARINT(std::vector<int>& buffer_to_write, const string OUT_ROOT);

void writeDataUTF(vector<int>& vertices_to_write,
                	vector<int>& indices_to_write,
                  const string OUT_ROOT);

void writeDataPNG(vector<int>& buffer_to_write,
                const string OUT_ROOT);

}

#endif