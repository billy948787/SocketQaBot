cmake_minimum_required(VERSION 3.10)

project(SocketQaBotServer)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES
    src/*.cpp
)
add_executable(SocketQaBotServer ${SOURCES})

# if compiler is clang, set compiler options to experimental
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
add_compile_options(
    -fexperimental-library
    -save-temps
)
endif()

if(WIN32)
    target_link_libraries(SocketQaBotServer PRIVATE ws2_32)
endif()



include_directories(
    ${CMAKE_SOURCE_DIR}/include
)
