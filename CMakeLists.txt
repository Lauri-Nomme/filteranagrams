cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_COMPILER "gcc")
project(fa C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native  -ftree-vectorize -ftree-vectorizer-verbose=1")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -march=native")
add_executable(fa main.c main.h)
target_link_libraries(fa pthread)
