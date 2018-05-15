add_library(PrototypePass MODULE
    Prototype.cpp
    DynamicTrace.cpp
    Replay.cpp
    CCA.cpp
    StreamAnalyzeTrace.cpp
    LocateAccelerableFunctions.cpp
    GZUtil.cpp
)


# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(PrototypePass PROPERTIES
    COMPILE_FLAGS "-std=c++14 -fno-rtti"
)

target_link_libraries(PrototypePass -lz)

# Protobuf tracer library.
protobuf_generate_cpp(PROTOBUF_SRCS PROTOBUF_HDRS
    TraceMessage.proto
)

add_library(TracerProtobuf STATIC 
    Tracer.cpp
    TracerProtobuf.cpp
    ${PROTOBUF_SRCS}
)
target_include_directories(TracerProtobuf PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# GZip tracer library
add_library(TracerGZip STATIC
    Tracer.cpp
    TracerGZip.c
)