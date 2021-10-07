cmake_minimum_required(VERSION 3.4)
project("GemForge")

set(CMAKE_CXX_STANDARD 14)

if (NOT DEFINED ENV{LLVM_DEBUG_INSTALL_PATH})
    message(FATAL_ERROR "Please specify where debug version of llvm installed in LLVM_DEBUG_INSTALL_PATH")
endif ()
set(LLVM_PATH "$ENV{LLVM_DEBUG_INSTALL_PATH}")

message(STATUS "LLVM Debug Install: " ${LLVM_PATH})
find_package(LLVM REQUIRED 
    CONFIG
    PATHS ${LLVM_PATH}
    NO_DEFAULT_PATH
)

if (NOT DEFINED ENV{PROTOBUF_INSTALL_PATH})
    message(FATAL_ERROR "Please specify where protobuf is installed in PROTOBUF_INSTALL_PATH")
endif ()
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH}
    $ENV{PROTOBUF_INSTALL_PATH})

set(Protobuf_USE_STATIC_LIBS ON)
find_package(Protobuf 
    REQUIRED
)

add_definitions(${LLVM_DEFINITIONS})
include_directories(
    ${LLVM_INCLUDE_DIRS} 
    ${PROTOBUF_INCLUDE_DIRS}
    src
    )
link_directories(${LLVM_LIBRARY_DIRS})

# Make a python protobuf folder
set(PYTHON_PROTO_MSG "py-proto-msg")
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/${PYTHON_PROTO_MSG})

# Generate the compile commands.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_subdirectory(src)
add_subdirectory(test)

enable_testing()
add_test(NAME whatever COMMAND GemForgeTest)
