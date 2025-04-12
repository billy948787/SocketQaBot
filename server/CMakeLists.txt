cmake_minimum_required(VERSION 3.10)

project(SocketQaBotServer)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

# if compiler is clang, set compiler options to experimental
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
add_compile_options(
    -fexperimental-library
    -save-temps
)
endif()

file(GLOB_RECURSE SOURCES
    src/*.cpp
)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

add_executable(SocketQaBotServer ${SOURCES})