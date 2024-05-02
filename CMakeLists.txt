cmake_minimum_required(VERSION 3.28)
project(C64Fuse VERSION 0.0.1 LANGUAGES C)

include(FindPkgConfig)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)

include (CTest)

add_subdirectory(d64-fuse)
add_subdirectory(DiskImagery64-base)
