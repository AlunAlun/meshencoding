cmake_minimum_required(VERSION 3.0.2)
project(c_export)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(SOURCE_FILES main.cpp tiny_obj_loader.cc Geometry.cpp Export.cpp )
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "/Users/alun/Dropbox/Work/Code/Projects/meshencoding/c_export/binaries")
add_executable(c_export ${SOURCE_FILES})

include_directories(/opt/local/include)

target_link_libraries(c_export /opt/local/lib/libpng.dylib)




