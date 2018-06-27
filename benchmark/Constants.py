
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
musl libc path.
It must be compiled with gllvm/wllvm to get the LLVM bitcode.
"""
MUSL_LIBC_PATH = '/home/zhengrong/Documents/musl-1.1.19/lib'
MUSL_LIBC_LLVM_BC = os.path.join(MUSL_LIBC_PATH, 'libc.a.bc')
MUSL_LIBC_CRT = os.path.join(MUSL_LIBC_PATH, 'crt1.o')
MUSL_LIBC_STATIC_LIB = os.path.join(MUSL_LIBC_PATH, 'libc.a')

"""
LLVM Compiler run time path.
Provide the compiler run time library.
"""
LLVM_BUILD_PATH = '/home/zhengrong/Documents/llvm-6.0/llvm/build'
LLVM_COMPILER_RT_PATH = os.path.join(LLVM_BUILD_PATH, 'lib/clang/6.0.0/lib/linux')
LLVM_COMPILER_RT_BUILTIN_LIB = os.path.join(LLVM_COMPILER_RT_PATH, 'libclang_rt.builtins-x86_64.a')
LLVM_UNWIND_STATIC_LIB = os.path.join(LLVM_BUILD_PATH, 'lib/libunwind.a')

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


"""
Gem5 parameters.
"""
CPU_TYPE = 'DerivO3CPU'
ISSUE_WIDTH = 1
STORE_QUEUE_SIZE = 32
GEM5_L1D_SIZE = '32kB'
GEM5_L1I_SIZE = '32kB'
GEM5_L2_SIZE = '256kB'
