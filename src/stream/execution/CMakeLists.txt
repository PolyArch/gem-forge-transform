
# Generate the stream intrinsic LLVM IR.
add_custom_target(StreamMemIntrinsicIR ALL
    clang -O3 -mavx512f -S -emit-llvm ${CMAKE_CURRENT_SOURCE_DIR}/StreamMemIntrinsic.c
    COMMAND opt -instnamer
        StreamMemIntrinsic.ll
        -S -o StreamMemIntrinsic.ll
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
    COMMENT "Generate StreamMem Intrinsic"
    SOURCES StreamMemIntrinsic.c
)
add_dependencies(StreamMemIntrinsicIR LLVMTDGPass)