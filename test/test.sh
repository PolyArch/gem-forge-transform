#!/bin/bash 

# Build the normal binary (32 bit)
# $1: The file you want to compile (without .c suffix).
# ret: NORMAL_BINARY_NAME.
function build_normal_binary {
    local BASE_NAME=${1}
    NORMAL_BINARY_NAME=${BASE_NAME}
    make ${NORMAL_BINARY_NAME}
}

# Generate the llvm trace binary.
# $1: The file you want to trace (without .c suffix).
# ret: TRACE_BINARY_NAME. It will generate the llvm trace to stdout.
function build_llvm_trace_binary {
    local TRACE_BINARY_NAME=${1}
    make ${TRACE_BINARY_NAME}
}

function build_llvm_replay_binary {
    local REPLAY_BINARY_NAME=${1}
    make ${REPLAY_BINARY_NAME}
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
    local CPU_TYPE="TimingSimpleCPU"
    local TRACE_FILE_NAME=${3}
    local DEBUG_TYPE="--debug-flags=LLVMTraceCPU"
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
        ${GEM5_X86} --outdir=${GEM5_OUT_DIR} ${DEBUG_TYPE} ${GEM5_SE_CONFIG} --cmd=${BINARY_NAME} --llvm-trace-file=${TRACE_FILE_NAME} --caches --l2cache --cpu-type=${CPU_TYPE} --l1d_size=${L1D_SIZE} --l1i_size=${L1I_SIZE} --l2_size=${L2_SIZE} --l3_size=${L3_SIZE}
    else
        ${GEM5_X86} --outdir=${GEM5_OUT_DIR} ${DEBUG_TYPE} ${GEM5_SE_CONFIG} --cmd=${BINARY_NAME} --cpu-type=${CPU_TYPE} --llvm-trace-file=${TRACE_FILE_NAME}
    fi
}

# We are in the root directory. Rebuild the pass.
HOME=${PWD}
cd build
cmake ..
make
cd ../test

USE_CACHE=1

WORKDIR="MachSuite/fft/transpose"
# WORKDIR="MachSuite/fft/strided"
# WORKDIR="MachSuite/fft/strided-raw"
# WORKDIR="MachSuite/kmp/kmp"
# WORKLOAD="kmp"
WORKLOAD="fft"

TRACE_BINARY_NAME="${WORKLOAD}_trace"
TRACE_FILE_NAME="${TRACE_BINARY_NAME}.output"

GEM5_LLVM_TRACE_CPU_FILE="${WORKLOAD}_gem5_llvm_trace.txt"

REPLAY_BINARY_NAME="${WORKLOAD}_replay"

cd ${WORKDIR}

# Clean everything.
make clean

# build normal binary.
# build_normal_binary ${WORKLOAD}

# run gem5.
# run_gem5 ${NORMAL_BINARY_NAME} ${USE_CACHE}

# Generate the trace binary.
build_llvm_trace_binary ${TRACE_BINARY_NAME}

# Run the binary to get the trace.
./${TRACE_BINARY_NAME} > ${TRACE_FILE_NAME}

# Parse the trace and generate result used for gem5 LLVMTraceCPU
python ${HOME}/util/datagraph.py ${TRACE_FILE_NAME} pr_gem5 ${GEM5_LLVM_TRACE_CPU_FILE}

# Simulate with LLVMTraceCPU
build_llvm_replay_binary ${REPLAY_BINARY_NAME}
run_gem5_llvm_trace_cpu ${REPLAY_BINARY_NAME} ${USE_CACHE} ${GEM5_LLVM_TRACE_CPU_FILE}

cd ${HOME}
