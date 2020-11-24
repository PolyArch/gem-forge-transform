
set(LLVM_SRC_LIB_DIR "$ENV{LLVM_SRC_LIB_PATH}")
if (${LLVM_SRC_LIB_DIR} STREQUAL "")
  message(fatal_error "Please specify where is source tree of llvm")
endif()

# First dive into trace, as it will give us PROTOBUF variables.
add_subdirectory(trace)
add_subdirectory(stream)

# generate tdg protobuf messages.
protobuf_generate_cpp(TDG_PROTOBUF_SRCS TDG_PROTOBUF_HDRS
    TDGInstruction.proto
)
protobuf_generate_python(TDG_PROTOBUF_PY TDGInstruction.proto)

# The llvm pass shared library.
add_library(LLVMTDGPass SHARED
    strip_m5/StripM5.cpp
    trace/TracePass.cpp
    trace/GZipUtil.cpp
    trace/TraceParserProtobuf.cpp
    trace/ProfileParser.cpp
    trace/InstructionUIDMap.cpp
    trace/ProfileAnalyzePass.cpp
    trace/ProfileLogger.cpp
    trace/SimpointIntervalSelectPass.cpp
    trace/CallLoopProfileTree.cpp
    $<TARGET_OBJECTS:ProtobufTraceMessage>
    ${TDG_PROTOBUF_SRCS}
    DynamicInstruction.cpp
    DataGraph.cpp
    Utils.cpp
    TransformUtils.cpp
    DeadCodeElimination.cpp
    UserSpecifiedInstructionSet.cpp
    LoopUtils.cpp
    LoopUnroller.cpp
    BasicBlockPredicate.cpp
    PostDominanceFrontier.cpp
    ExecutionDataGraph.cpp
    DynamicLoopTree.cpp
    MemoryFootprint.cpp
    LocateAccelerableFunctions.cpp
    GemForgeBasePass.cpp
    Replay.cpp
    TraceStatisticPass.cpp
    ProtobufSerializer.cpp
    Gem5ProtobufSerializer.cpp
    TDGSerializer.cpp
    CacheWarmer.cpp
    RegionStatRecorder.cpp
    # SIMD
    simd/SIMDPass.cpp
    # Abstract data flow accelerator
    adfa/AbstractDataFlowAcceleratorPass.cpp
    # Stream
    $<TARGET_OBJECTS:ProtobufStreamMessage>
    stream/StreamUtils.cpp
    stream/StreamPattern.cpp
    stream/InlineContextStreamPass.cpp
    stream/InlineContext.cpp
    stream/InlineContextStream.cpp
    stream/StaticStream.cpp
    stream/StaticIndVarStream.cpp
    stream/StaticMemStream.cpp
    stream/UserDefinedMemStream.cpp
    stream/StreamConfigureLoopInfo.cpp
    stream/StaticStreamRegionAnalyzer.cpp
    stream/DynStream.cpp
    stream/DynIndVarStream.cpp
    stream/DynMemStream.cpp
    stream/StreamTransformPlan.cpp
    stream/DynStreamRegionAnalyzer.cpp
    stream/StreamPassOptions.cpp
    stream/StreamPass.cpp
    stream/StreamPrefetchPass.cpp
    stream/ae/StreamDataGraph.cpp
    stream/ae/FunctionalStreamPattern.cpp
    stream/ae/FunctionalStream.cpp
    stream/ae/FunctionalStreamEngine.cpp
    stream/ae/DynamicStreamCoalescer.cpp
    stream/execution/StreamExecutionTransformer.cpp
    stream/execution/StreamExecutionPass.cpp
    stream/execution/StreamExecutionStaticPass.cpp
    # CCA.cpp
    # StreamAnalyzeTrace.cpp
    # Speculative Precomputation.
    speculative_precomputation/PrecomputationSlice.cpp
    speculative_precomputation/CriticalLoadManager.cpp
    speculative_precomputation/SpeculativePrecomputationPass.cpp
    valid/ValidExecutionPass.cpp
)


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

message(STATUS ${PROTOBUF_LIBRARY})
target_link_libraries(LLVMTDGPass -lz ${PROTOBUF_LIBRARY} ${LLVM_DEP_LIBS})

# Copy the generated python protobuf file to benchmark/Utils/.
add_custom_target(PyTDGProtobufSrc ALL
    cp ${TDG_PROTOBUF_PY} ${PROJECT_SOURCE_DIR}/driver/Utils/
    DEPENDS ${TDG_PROTOBUF_PY})

