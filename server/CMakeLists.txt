cmake_minimum_required(VERSION 3.10)

project(SocketQaBotServer)


set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES
    src/*.cpp
)
add_executable(SocketQaBotServer ${SOURCES})

include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)


# if compiler is clang, set compiler options to experimental
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
add_compile_options(
    -fexperimental-library
    -save-temps
)
endif()
    
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)
        
if(WIN32)
    target_link_libraries(SocketQaBotServer PRIVATE ws2_32)
endif()
        
target_link_libraries(SocketQaBotServer PRIVATE nlohmann_json::nlohmann_json)
