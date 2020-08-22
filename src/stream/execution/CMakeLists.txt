
# Generate the stream_memset LLVM IR.
add_custom_target(StreamMemsetIR ALL
    clang -O3 -mavx512f -S -emit-llvm ${CMAKE_CURRENT_SOURCE_DIR}/StreamMemset.c
    COMMAND opt -instnamer
        StreamMemset.ll
        -S -o StreamMemset.ll
    # COMMAND opt -load $<TARGET_FILE:LLVMTDGPass>
    #     -trace-pass
    #     -trace-inst-uid-file StreamMemset.uid
    #     StreamMemset.ll
    #     -trace-function stream_memset
    #     -S -o StreamMemset.trace.ll
    # COMMAND opt -load $<TARGET_FILE:LLVMTDGPass>
    #     -stream-execution-static-pass
    #     -stream-pass-choose-strategy=static-outer
    #     -stream-pass-allow-aliased-stream
    #     -stream-pass-enable-store 
    #     StreamMemset.ll
    #     -gem-forge-inst-uid-file StreamMemset.uid
    #     -S -o StreamMemset.ssp.ll
    COMMENT "Generate StreamMemset Intrinsic"
    SOURCES StreamMemset.c
)
add_dependencies(StreamMemsetIR LLVMTDGPass)