

# First dive into trace, as it will give us PROTOBUF variables.
add_subdirectory(trace)

# generate tdg protobuf messages.
protobuf_generate_cpp(TDG_PROTOBUF_SRCS TDG_PROTOBUF_HDRS
    TDGInstruction.proto
)

message(STATUS "TESTING TRACE_PROTOBUF_SRCS")
message(STATUS ${TRACE_PROTOBUF_SRCS})

SET_SOURCE_FILES_PROPERTIES(
    ${TRACE_PROTOBUF_SRCS} 
    ${TRACE_PROTOBUF_HDRS} 
    ${TDG_PROTOBUF_SRCS} 
    ${TDG_PROTOBUF_HDRS} 
    PROPERTIES GENERATED TRUE
)

# The llvm pass shared library.
add_library(LLVMTDGPass MODULE
    trace/TracePass.cpp
    trace/TraceParserGZip.cpp
    trace/GZipUtil.cpp
    trace/TraceParserProtobuf.cpp
    ${TRACE_PROTOBUF_SRCS}
    ${TDG_PROTOBUF_SRCS}
    DynamicInstruction.cpp
    DataGraph.cpp
    LoopUtils.cpp
    PostDominanceFrontier.cpp
    LocateAccelerableFunctions.cpp
    Replay.cpp
    TraceStatisticPass.cpp
    TDGSerializer.cpp
    # SIMD
    simd/SIMDPass.cpp
    # Abstract data flow accelerator
    adfa/AbstractDataFlowAcceleratorPass.cpp
    # CCA.cpp
    # StreamAnalyzeTrace.cpp
)

# Make sure LLVMTDGPass is dependent on protobuf tracer library,
# as we need the generated protobuf src file.
add_dependencies(LLVMTDGPass TracerProtobuf)

# Make sure we can find the protobuf headers.
target_include_directories(LLVMTDGPass 
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/trace
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(LLVMTDGPass PROPERTIES
    COMPILE_FLAGS "-std=c++14 -g -fno-rtti -DGOOGLE_PROTOBUF_NO_RTTI"
)

target_link_libraries(LLVMTDGPass -lz ${PROTOBUF_LIBRARY})