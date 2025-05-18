cmake_minimum_required(VERSION 3.30)

project(SocketQaBotServer)


set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_STANDARD_REQUIRED ON)


if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
add_compile_options(
    -fexperimental-library
    -save-temps
)
endif()

file(GLOB_RECURSE SOURCES
    src**/*.cpp
)
add_executable(SocketQaBotServer ${SOURCES})

include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

find_package(OpenSSL REQUIRED)


# if compiler is clang, set compiler options to experimental
    
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

if(WIN32)
    target_link_libraries(SocketQaBotServer PRIVATE ws2_32)
endif()
        
target_link_libraries(SocketQaBotServer PRIVATE nlohmann_json::nlohmann_json)

target_link_libraries(SocketQaBotServer PRIVATE OpenSSL::SSL OpenSSL::Crypto)