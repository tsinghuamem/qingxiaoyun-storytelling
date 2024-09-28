cmake_minimum_required(VERSION 3.13)
project(ai_story)

set(CMAKE_C_STANDARD 99)
set(CMAKE_BUILD_TYPE Debug)

set(INC_DIR ${PROJECT_SOURCE_DIR}/inc)
set(LIB_DIR ${PROJECT_SOURCE_DIR}/lib)
set(OUTPUT_DIR ${PROJECT_SOURCE_DIR}/bin)

add_subdirectory(src)