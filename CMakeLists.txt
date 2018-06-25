cmake_minimum_required(VERSION 3.4)

set(CMAKE_CXX_COMPILER ecc++)
set(CMAKE_C_COMPILER ecc)
set(CMAKE_AR ecc-ar)
set(CMAKE_RANLIB ecc-ranlib)

find_package(LLVM REQUIRED 
    CONFIG
    PATHS /home/zhengrong/Documents/ellcc/build/build-ecc/llvm/x86_64-linux
    NO_DEFAULT_PATH
    )

# Use our own version of protobuf.
set(CMAKE_PREFIX_PATH $(CMAKE_PREFIX_PATH) /home/zhengrong/Documents/protobuf-ellcc)
message(STATUS ${CMAKE_PREFIX_PATH})
find_package(Protobuf 
    REQUIRED
)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS} ${PROTOBUF_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

add_subdirectory(prototype)