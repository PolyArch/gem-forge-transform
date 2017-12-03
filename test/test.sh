#!/bin/bash 

# Build the normal binary (32 bit)
# $1: The file you want to compile (without .c suffix).
# ret: NORMAL_BINARY_NAME.
function build_normal_binary {
    local BASE_NAME=${1}
    local BASE_C_NAME="${BASE_NAME}.c"
    local BASE_LLVM_NAME="${BASE_NAME}.ll"
    # Emit the LLVM file.
    clang -emit-llvm -S -o ${BASE_LLVM_NAME} -c ${BASE_C_NAME}
    local ASM_NAME="${BASE_NAME}.s"
    llc -filetype=asm -o ${ASM_NAME} ${BASE_LLVM_NAME}
    NORMAL_BINARY_NAME=${BASE_NAME}
    gcc -fno-inline -o ${NORMAL_BINARY_NAME} ${ASM_NAME}
}

# Generate the llvm trace binary.
# $1: The file you want to trace (without .c suffix).
# ret: BINARY_NAME. It will generate the llvm trace to stdout.
function build_llvm_trace_binary {
    local BASE_NAME=${1}
    local BASE_C_NAME="${BASE_NAME}.c"
    local BASE_LLVM_NAME="${BASE_NAME}.ll"
    local BASE_LLVM_TRACE_NAME="${BASE_NAME}_trace.ll"
    # Emit the LLVM file.
    clang -emit-llvm -S -o ${BASE_LLVM_NAME} -c ${BASE_C_NAME}
    clang -emit-llvm -S -o logger.ll -c logger.c
    # Run the instrument pass.
    local OPT_DEBUG="PrototypePass"
    local OPT_ARGS="-load=../build/prototype/libPrototypePass.so -S -prototype -time-passes -debug-only=${OPT_DEBUG} "
    opt ${OPT_ARGS} -o ${BASE_LLVM_TRACE_NAME} ${BASE_LLVM_NAME}
    # Link them together.
    local BYTE_CODE_NAME="${BASE_NAME}_trace.bc"
    local ASM_NAME="${BASE_NAME}_trace.s"
    TRACE_BINARY_NAME="${BASE_NAME}_trace"
    llvm-link -o ${BYTE_CODE_NAME} ${BASE_LLVM_TRACE_NAME} logger.ll
    llc -filetype=asm -o ${ASM_NAME} ${BYTE_CODE_NAME}
    gcc -fno-inline -o ${TRACE_BINARY_NAME} ${ASM_NAME}
}

# Run the binary in gem5.
function run_gem5 {
    local BINARY_NAME=${1}
    local USE_CACHE=${2}
    local CPU_TYPE="TimingSimpleCPU"
    local GEM5_OUT_DIR="${BINARY_NAME}.${CPU_TYPE}.m5out"
    mkdir -p ${GEM5_OUT_DIR}
    local GEM5_PATH="/home/sean/Public/gem5"
    local GEM5_X86="${GEM5_PATH}/build/X86/gem5.opt"
    local GEM5_SE_CONFIG="${GEM5_PATH}/configs/example/se.py"
    local L1D_SIZE="32kB"
    local L1I_SIZE="32kB"
    local L2_SIZE="256kB"
    local L3_SIZE="6144kB"
    if (( USE_CACHE == 1 )); then
        ${GEM5_X86} --outdir=${GEM5_OUT_DIR} ${GEM5_SE_CONFIG} --cmd=${BINARY_NAME} --caches --l2cache --cpu-type=${CPU_TYPE} --l1d_size=${L1D_SIZE} --l1i_size=${L1I_SIZE} --l2_size=${L2_SIZE} --l3_size=${L3_SIZE}
    else
        ${GEM5_X86} --outdir=${GEM5_OUT_DIR} ${GEM5_SE_CONFIG} --cmd=${BINARY_NAME} --cpu-type=${CPU_TYPE}
    fi
}

function run_gem5_llvm_trace_cpu {
    local BINARY_NAME=${1}
    local USE_CACHE=${2}
    local TRACE_FILE_NAME=${3}
    local CPU_TYPE="LLVMTraceCPU"
    local GEM5_OUT_DIR="${BINARY_NAME}.${CPU_TYPE}.m5out"
    mkdir -p ${GEM5_OUT_DIR}
    local GEM5_PATH="/home/sean/Public/gem5"
    local GEM5_X86="${GEM5_PATH}/build/X86/gem5.opt"
    local GEM5_SE_CONFIG="${GEM5_PATH}/configs/example/llvm_trace_replay.py"
    local L1D_SIZE="32kB"
    local L1I_SIZE="32kB"
    local L2_SIZE="256kB"
    local L3_SIZE="6144kB"
    if (( USE_CACHE == 1 )); then
        ${GEM5_X86} --outdir=${GEM5_OUT_DIR} --debug-flags=${CPU_TYPE} ${GEM5_SE_CONFIG} --llvm-trace-file=${TRACE_FILE_NAME} --caches --l2cache --cpu-type=${CPU_TYPE} --l1d_size=${L1D_SIZE} --l1i_size=${L1I_SIZE} --l2_size=${L2_SIZE} --l3_size=${L3_SIZE}
    else
        ${GEM5_X86} --outdir=${GEM5_OUT_DIR} --debug-flags=${CPU_TYPE} ${GEM5_SE_CONFIG} --llvm-trace-file=${TRACE_FILE_NAME} --cpu-type=${CPU_TYPE}
    fi
}

# We are in the root directory.
cd build
cmake ..
make
cd ../test

WORKLOAD="kmp/kmp"
# WORKLOAD="fft_stride/fft_stride"
# WORKLOAD="hello/hello"
USE_CACHE=0

# build normal binary.
# build_normal_binary "fft_stride/fft_stride"
build_normal_binary ${WORKLOAD}

# run gem5.
run_gem5 ${NORMAL_BINARY_NAME} ${USE_CACHE}

# Generate the trace binary.
build_llvm_trace_binary ${WORKLOAD}

# Run the binary to get the trace.
TRACE_FILE_NAME="${TRACE_BINARY_NAME}.output"
# ./${TRACE_BINARY_NAME} | tee ${TRACE_FILE_NAME}
./${TRACE_BINARY_NAME} > ${TRACE_FILE_NAME}

# Parse the trace and generate result used for gem5 LLVMTraceCPU
GEM5_LLVM_TRACE_CPU_FILE="${WORKLOAD}_gem5_llvm_trace.txt"
python ../util/datagraph.py ${TRACE_FILE_NAME} pr_gem5 ${GEM5_LLVM_TRACE_CPU_FILE}

# Simulate with LLVMTraceCPU
run_gem5_llvm_trace_cpu ${NORMAL_BINARY_NAME} ${USE_CACHE} ${GEM5_LLVM_TRACE_CPU_FILE}