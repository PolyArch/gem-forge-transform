
import os

# Where is llvm-tdg
LLVM_TDG_DIR = os.getenv('LLVM_TDG_PATH')
assert(LLVM_TDG_DIR is not None)
LLVM_TDG_BUILD_DIR = os.path.join(LLVM_TDG_DIR, 'build', 'src')
LLVM_TDG_DRIVER_DIR = os.path.join(LLVM_TDG_DIR, 'driver')
LLVM_TDG_BENCHMARK_DIR = os.path.join(LLVM_TDG_DIR, 'benchmark')

# Where to store the llvm_bc and traces.
EXPERIMENTS = 'stream'
# EXPERIMENTS = 'fractal'
LLVM_TDG_RESULT_DIR = os.path.join(
    os.getenv('LLVM_TDG_RESULT_PATH'), EXPERIMENTS)

LLVM_TDG_REPLAY_C = os.path.join(LLVM_TDG_DRIVER_DIR, 'replay.c')

"""
Normal llvm install path.
"""
LLVM_PATH = os.getenv('LLVM_RELEASE_INSTALL_PATH')
assert(LLVM_PATH is not None)
LLVM_BIN_PATH = os.path.join(LLVM_PATH, 'bin')
LLVM_DEBUG_PATH = os.getenv('LLVM_DEBUG_INSTALL_PATH')
LLVM_DEBUG_BIN_PATH = os.path.join(LLVM_DEBUG_PATH, 'bin')


"""
ellcc path.
"""

ELLCC_PATH = '/home/zhengrong/Documents/ellcc'
ELLCC_BIN_PATH = os.path.join(ELLCC_PATH, 'bin')

"""
Protobuf built with ellcc.
"""
PROTOBUF_ELLCC_LIB = '/home/zhengrong/Documents/protobuf-ellcc/lib/libprotobuf.a'

"""
Standard library path. For tracing standard library.
"""
STDLIB_PATH = os.path.join(LLVM_TDG_DIR, 'benchmark', 'stdlib')
MUSL_LIBC_LLVM_BC = os.path.join(STDLIB_PATH, 'libc.a.bc')
MUSL_LIBC_CRT = os.path.join(STDLIB_PATH, 'crt1.o')
MUSL_LIBC_STATIC_LIB = os.path.join(STDLIB_PATH, 'libc.a')

"""
LLVM Compiler run time path.
Provide the compiler run time library.
"""
LLVM_COMPILER_RT_BC = os.path.join(STDLIB_PATH, 'libcompiler-rt.a.bc')
LLVM_COMPILER_RT_LIB = os.path.join(STDLIB_PATH, 'libcompiler-rt.a')
# LLVM_UNWIND_STATIC_LIB = os.path.join(LLVM_BUILD_PATH, 'lib/libunwind.a')

"""
GEM5_PATH
"""
GEM5_DIR = os.getenv('LLVM_TRACE_CPU_PATH')
GEM5_INCLUDE_DIR = os.path.join(GEM5_DIR, 'include')
GEM5_X86 = os.path.join(GEM5_DIR, 'build/X86/gem5.opt')
GEM5_LLVM_TRACE_SE_CONFIG = os.path.join(
    GEM5_DIR, 'configs/example/llvm_trace_replay.py')
GEM5_SE_CONFIG = os.path.join(
    GEM5_DIR, 'configs/example/se.py')
GEM5_M5OPS_X86 = os.path.join(GEM5_DIR, 'util', 'm5', 'm5op_x86.S')

HOFFMAN2_GEM5_DIR = '/u/home/s/seanzw/llvm-trace-cpu'
HOFFMAN2_GEM5_X86 = os.path.join(HOFFMAN2_GEM5_DIR, 'build/X86/gem5.opt')
HOFFMAN2_GEM5_LLVM_TRACE_SE_CONFIG = os.path.join(
    HOFFMAN2_GEM5_DIR, 'configs/example/llvm_trace_replay.py')
HOFFMAN2_GEM5_SE_CONFIG = os.path.join(
    HOFFMAN2_GEM5_DIR, 'configs/example/se.py')

"""
Gem5 parameters.
"""
CPU_TYPE = 'DerivO3CPU'
ISSUE_WIDTH = 8
STORE_QUEUE_SIZE = 32
GEM5_L1D_SIZE = '32kB'
GEM5_L1D_MSHR = 16
GEM5_L1I_SIZE = '32kB'
GEM5_L2_SIZE = '256kB'
GEM5_USE_MCPAT = 0


USE_ELLCC = False

if not USE_ELLCC:
    OPT = os.path.join(LLVM_DEBUG_BIN_PATH, 'opt')
    CC = os.path.join(LLVM_BIN_PATH, 'clang')
    CXX = os.path.join(LLVM_BIN_PATH, 'clang++')
    LLVM_LINK = os.path.join(LLVM_BIN_PATH, 'llvm-link')
    LLVM_DIS = os.path.join(LLVM_BIN_PATH, 'llvm-dis')
    # Some one installed a wrong version of protobuf on my computer.
    # PROTOBUF_LIB = os.path.join('/usr/local/lib/libprotobuf.a')
    PROTOBUF_LIB = os.getenv('LIBPROTOBUF_STATIC_LIB')
    if PROTOBUF_LIB is None:
        print('Missing env var LIBPROTOBUF_STATIC_LIB')
        assert(False)
    LIBUNWIND_LIB = os.path.join('-lunwind')
else:
    OPT = os.path.join(ELLCC_BIN_PATH, 'opt')
    CC = os.path.join(ELLCC_BIN_PATH, 'ecc')
    CXX = os.path.join(ELLCC_BIN_PATH, 'ecc++')
    LLVM_LINK = os.path.join(ELLCC_BIN_PATH, 'llvm-link')
    LLVM_DIS = os.path.join(ELLCC_BIN_PATH, 'llvm-dis')
    PROTOBUF_LIB = PROTOBUF_ELLCC_LIB
    print("Undefined libunwind for ellcc!")
    assert(False)

HOFFMAN2_SSH_SCRATCH = 'seanzw@hoffman2.idre.ucla.edu:/u/flashscratch/s/seanzw'
HOFFMAN2_SCRATCH = '/u/flashscratch/s/seanzw'
