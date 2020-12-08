cmake_minimum_required(VERSION 3.4)
project("GemForge")

set(ELLCC_PATH /home/zhengrong/Documents/ellcc CACHE PATH "Where to find ellcc.")
set(ELLCC_LLVM_PATH /home/zhengrong/Documents/ellcc/build/build-ecc/llvm/x86_64-linux CACHE PATH "Where to find llvm when use ellcc.")
option(USE_ELLCC "Use ellcc to trace stdlib" OFF)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH}
    $ENV{PROTOBUF_INSTALL_PATH})

set(LLVM_PATH "$ENV{LLVM_DEBUG_INSTALL_PATH}")
if (${LLVM_PATH} STREQUAL "")
  message(fatal_error "Please specify there is debug version of llvm installed")
endif()

if (USE_ELLCC)
    set(CMAKE_CXX_COMPILER $(ELLCC_PATH)/bin/ecc++)
    set(CMAKE_C_COMPILER $(ELLCC_PATH)/bin/ecc)
    set(CMAKE_AR $(ELLCC_PATH)/bin/ecc-ar)
    set(CMAKE_RANLIB $(ELLCC_PATH)/bin/ecc-ranlib)
    set(LLVM_PATH $(ELLCC_LLVM_PATH))
endif (USE_ELLCC)

message(STATUS ${LLVM_PATH})
find_package(LLVM REQUIRED 
    CONFIG
    PATHS ${LLVM_PATH}
    NO_DEFAULT_PATH
)

# Use our own version of protobuf if using ellcc.
if (USE_ELLCC)
    set(CMAKE_PREFIX_PATH $(CMAKE_PREFIX_PATH) /home/zhengrong/Documents/protobuf-ellcc)
endif (USE_ELLCC)

message(STATUS ${CMAKE_PREFIX_PATH})

set(Protobuf_USE_STATIC_LIBS ON)
message(STATUS ${CMAKE_FIND_LIBRARY_SUFFIXES})
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

# Generate the compile commands.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_subdirectory(src)
add_subdirectory(test)

enable_testing()
add_test(NAME whatever COMMAND GemForgeTest)
