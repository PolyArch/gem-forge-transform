
import os

LLVM_TDG_DIR = '/home/zhengrong/Documents/LLVM-TDG'
LLVM_TDG_BUILD_DIR = os.path.join(LLVM_TDG_DIR, 'build', 'src')

"""
ellcc path.
"""

ELLCC_PATH = '/home/zhengrong/Documents/ellcc'

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
GEM5_DIR = '/home/zhengrong/Documents/llvm-trace-cpu'
GEM5_INCLUDE_DIR = os.path.join(GEM5_DIR, 'include')
GEM5_X86 = os.path.join(GEM5_DIR, 'build/X86/gem5.opt')
GEM5_LLVM_TRACE_SE_CONFIG = os.path.join(
    GEM5_DIR, 'configs/example/llvm_trace_replay.py')
GEM5_SE_CONFIG = os.path.join(
    GEM5_DIR, 'configs/example/se.py')
GEM5_M5OPS_X86 = os.path.join(GEM5_DIR, 'util', 'm5', 'm5op_x86.S')


"""
Gem5 parameters.
"""
CPU_TYPE = 'DerivO3CPU'
ISSUE_WIDTH = 4
STORE_QUEUE_SIZE = 32
GEM5_L1D_SIZE = '32kB'
GEM5_L1I_SIZE = '32kB'
GEM5_L2_SIZE = '256kB'
