#!/bin/bash 

# Run the binary in gem5.
function run_gem5 {
    local BINARY_NAME=${1}
    local USE_CACHE=${2}
    local CPU_NUM=${3}
    local BINARY_ARGS=${4}
    local CPU_TYPE="DerivO3CPU"
    # local DEBUG_TYPE="--debug-flags=LSQUnit"
    local DEBUG_TYPE=""
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
        ${GEM5_X86} --outdir=${GEM5_OUT_DIR} ${DEBUG_TYPE} ${GEM5_SE_CONFIG} --cmd=${BINARY_NAME} --options="${BINARY_ARGS}" --caches --l2cache --cpu-type=${CPU_TYPE} --num-cpus=${CPU_NUM} --l1d_size=${L1D_SIZE} --l1i_size=${L1I_SIZE} --l2_size=${L2_SIZE} --l3_size=${L3_SIZE}
    else
        ${GEM5_X86} --outdir=${GEM5_OUT_DIR} ${DEBUG_TYPE} ${GEM5_SE_CONFIG} --cmd=${BINARY_NAME} --options="${BINARY_ARGS}" --cpu-type=${CPU_TYPE} --num-cpus=${CPU_NUM}
    fi
}

function run_gem5_llvm_trace_cpu {
    local BINARY_NAME=${1}
    local USE_CACHE=${2}
    local CPU_TYPE="DerivO3CPU"
    local TRACE_FILE_NAME=${3}
    local DEBUG_TYPE="--debug-flags=LLVMTraceCPU"
    local GEM5_OUT_DIR="${BINARY_NAME}.${CPU_TYPE}.${TRACE_FILE_NAME}.m5out"
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

function test_dot_exporter {
    local TRACE_FILE_NAME=${1}
    local DOT_FILE_NAME="dg.dot"
    local PNG_FILE_NAME="dg.png"
    python ${HOME}/util/DotGraphExporter.py ${TRACE_FILE_NAME} ${DOT_FILE_NAME}
    dot -Tpng ${DOT_FILE_NAME} -o${PNG_FILE_NAME}
}

function test_CCA {
    local TRACE_FILE_NAME=${1}
    local GEM5_LLVM_TRACE_CPU_FILE=${2}
    python ${HOME}/util/CCATransform.py ${TRACE_FILE_NAME} ${GEM5_LLVM_TRACE_CPU_FILE}
}

# We are in the root directory. Rebuild the pass.
HOME=${PWD}
cd build
cmake ..
make
cd ../test

USE_CACHE=1
CPU_NUM=1
BINARY_ARGS=""

# WORKDIR="MachSuite/fft/transpose"
# WORKDIR="MachSuite/fft/strided"
# WORKDIR="MachSuite/fft/strided-raw"
# WORKDIR="MachSuite/fft/strided-pseudo"
# WORKLOAD="fft"
# WORKDIR="MachSuite/kmp/kmp"
# WORKLOAD="kmp"
WORKDIR="hello"
WORKLOAD="hello"

# WORKDIR="testscratch"
# WORKLOAD="scratch"

NORMAL_BINARY_NAME=${WORKLOAD}

TRACE_BINARY_NAME="${WORKLOAD}_trace"
TRACE_FILE_NAME="llvm_trace.txt"

GEM5_LLVM_TRACE_CPU_FILE="llvm_trace_gem5.txt"
GEM5_LLVM_TRACE_CPU_TRANSFORMED_FILE="${WORKLOAD}_gem5_llvm_trace_transformed.txt"

REPLAY_BINARY_NAME="${WORKLOAD}_replay"

cd ${WORKDIR}

# Clean everything.
make clean
make clean_trace
make clean_replay

# run_gem5 "/home/sean/a.out" ${USE_CACHE} 4 ""
# PARSEC_ROOT="/home/sean/Documents/parsec-3.0/pkgs/apps/raytrace/obj/amd64-linux.gcc"
# run_gem5 "${PARSEC_ROOT}/bin/rtview" ${USE_CACHE} 4 "${PARSEC_ROOT}/bunny.obj -automove -nthreads 2 -frames 1 -res 16 16"
# run_gem5 "${PARSEC_ROOT}/bin/rtview" ${USE_CACHE} 4 "${PARSEC_ROOT}/happy_buddha.obj -automove -nthreads 2 -frames 3 -res 480 270"
# run_gem5 "/home/sean/test_gem5_multithread/test" ${USE_CACHE} 4 ""

# build normal binary.
make ${WORKLOAD}

# run gem5.
run_gem5 ${NORMAL_BINARY_NAME} ${USE_CACHE} ${CPU_NUM} ${BINARY_ARGS}

# Generate the trace binary.
make ${TRACE_BINARY_NAME}

# Run the binary to get the trace.
./${TRACE_BINARY_NAME} 

# Parse the trace and generate result used for gem5 LLVMTraceCPU
# python ${HOME}/util/datagraph.py ${TRACE_FILE_NAME} pr_gem5 ${GEM5_LLVM_TRACE_CPU_FILE}
# test_CCA ${TRACE_FILE_NAME} ${GEM5_LLVM_TRACE_CPU_TRANSFORMED_FILE}

# Simulate with LLVMTraceCPU
make ${REPLAY_BINARY_NAME}
run_gem5_llvm_trace_cpu ${REPLAY_BINARY_NAME} ${USE_CACHE} ${GEM5_LLVM_TRACE_CPU_FILE}
# run_gem5_llvm_trace_cpu ${REPLAY_BINARY_NAME} ${USE_CACHE} ${GEM5_LLVM_TRACE_CPU_TRANSFORMED_FILE}

# test_dot_exporter ${TRACE_FILE_NAME}

cd ${HOME}
