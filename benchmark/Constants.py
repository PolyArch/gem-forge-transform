
import os

LLVM_TDG_DIR = '/home/zhengrong/Documents/LLVM-TDG'

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
