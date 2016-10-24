CC=clang++
SOURCES=main.cpp tiny_obj_loader.cc Geometry.cpp filewriter.cc webmesh.cpp
INC=-I/usr/local/include
CFLAGS=-std=c++11 -stdlib=libc++
LIB=-lpng

all:
	$(CC) $(CFLAGS) $(INC) $(SOURCES) $(LIB) -o assets/webmesh

debug:
	$(CC) -g -Wall $(CFLAGS) $(INC) $(SOURCES) $(LIB) -o assets/webmesh

clean:
	rm *.o assets/*.o webmesh assets/webmesh