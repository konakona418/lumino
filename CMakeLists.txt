cmake_minimum_required(VERSION 3.10.0)
project(lumino VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

file(GLOB_RECURSE SOURCES src/*.c)
file(GLOB_RECURSE HEADERS src/*.h)

include_directories(src)

add_executable(lumino main.c ${SOURCES} ${HEADERS})

target_include_directories(lumino PRIVATE src)

