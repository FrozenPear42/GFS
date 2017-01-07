cmake_minimum_required(VERSION 3.6)
project(FS)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES main.c)
add_executable(FS ${SOURCE_FILES})

add_custom_command(TARGET FS PRE_BUILD
        COMMAND ${PROJECT_SOURCE_DIR}/inc_version ${PROJECT_SOURCE_DIR}/version.h)
