
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(LLVM_SRC_LIB_DIR "/home/zhengrong/Documents/llvm-6.0/llvm/lib")

# First dive into trace, as it will give us PROTOBUF variables.
add_subdirectory(trace)
add_subdirectory(stream)

# generate tdg protobuf messages.
protobuf_generate_cpp(TDG_PROTOBUF_SRCS TDG_PROTOBUF_HDRS
    TDGInstruction.proto
)

# The llvm pass shared library.
add_library(LLVMTDGPass MODULE
    trace/TracePass.cpp
    trace/TraceParserGZip.cpp
    trace/GZipUtil.cpp
    trace/TraceParserProtobuf.cpp
    trace/ProfileParser.cpp
    trace/InstructionUIDMap.cpp
    $<TARGET_OBJECTS:ProtobufTraceMessage>
    ${TDG_PROTOBUF_SRCS}
    DynamicInstruction.cpp
    DataGraph.cpp
    LoopUtils.cpp
    LoopUnroller.cpp
    PostDominanceFrontier.cpp
    DynamicLoopTree.cpp
    MemoryFootprint.cpp
    LocateAccelerableFunctions.cpp
    Replay.cpp
    TraceStatisticPass.cpp
    Gem5ProtobufSerializer.cpp
    TDGSerializer.cpp
    CacheWarmer.cpp
    # SIMD
    simd/SIMDPass.cpp
    # Abstract data flow accelerator
    adfa/AbstractDataFlowAcceleratorPass.cpp
    # Stream
    $<TARGET_OBJECTS:ProtobufStreamMessage>
    stream/StreamPattern.cpp
    stream/InlineContextStreamPass.cpp
    stream/InlineContext.cpp
    stream/InlineContextStream.cpp
    stream/Stream.cpp
    stream/InductionVarStream.cpp
    stream/MemStream.cpp
    stream/StreamPass.cpp
    stream/StreamPrefetchPass.cpp
    stream/ae/AddressDataGraph.cpp
    stream/ae/FunctionalStreamPattern.cpp
    stream/ae/FunctionalStream.cpp
    stream/ae/FunctionalStreamEngine.cpp
    # CCA.cpp
    # StreamAnalyzeTrace.cpp
)

# Make sure LLVMTDGPass is dependent on protobuf tracer library,
# as we need the generated protobuf src file.
# add_dependencies(LLVMTDGPass TracerProtobuf)
# add_dependencies(LLVMTDGPass ProtobufStreamMessage)

# Make sure we can find the protobuf headers.
target_include_directories(LLVMTDGPass 
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/trace
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(LLVMTDGPass PROPERTIES
    COMPILE_FLAGS "-std=c++14 -O3 -fno-rtti -DGOOGLE_PROTOBUF_NO_RTTI"
)

# Link to LLVM components.
llvm_map_components_to_libnames(LLVM_LIBS demangle interpreter)
llvm_expand_dependencies(LLVM_DEP_LIBS ${LLVM_LIBS})

# Inorder to use the actual header of interpreter, which is not part of the installed llvm headers.
target_include_directories(LLVMTDGPass PUBLIC ${LLVM_SRC_LIB_DIR})

target_link_libraries(LLVMTDGPass -lz ${PROTOBUF_LIBRARY} ${LLVM_DEP_LIBS})