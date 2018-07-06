import os
import subprocess

import Constants as C
import Util
from Benchmark import Benchmark


class MachSuiteBenchmarks:

    BENCHMARK_PARAMS = {
         "bfs": [
             "queue",
             # "bulk" # not working
         ],
         "aes": [
             "aes"
         ],
         "stencil": [
             "stencil2d",
             "stencil3d"
         ],
         "md": [
             "grid",
             "knn"
         ],
        "fft": [
            "strided",
            "transpose"
        ],
         "viterbi": [
             "viterbi"
         ],
         "sort": [
             #"radix",
             "merge"
         ],
         "spmv": [
             "ellpack",
             "crs"
         ],
         "kmp": [
             "kmp"
         ],
        #  "backprop": [
        #     "backprop" # Not working.
        #  ],
        "gemm": [
            "blocked",
            #  "ncubed"
        ],
         "nw": [
             "nw"
         ]
    }

    COMMON_SOURCES = [
        'local_support.c',
        '../../common/support.c',
        '../../common/harness.c',
    ]

    INCLUDE_DIR = '../../common'

    CFLAGS = [
        # Be careful, we do not support integrated mode with private global variable,
        # as there is no symbol for these variables and we can not find them during
        # replay.
        '-O2',
        '-Wall',
        '-Wno-unused-label',
        '-fno-inline-functions',
        '-fno-vectorize',
        '-fno-slp-vectorize',
        '-fno-unroll-loops'
    ]

    def __init__(self, folder):
        self.folder = folder
        self.cwd = os.getcwd()

        Util.call_helper(['mkdir', '-p', 'result'])

        self.benchmarks = dict()
        for benchmark in MachSuiteBenchmarks.BENCHMARK_PARAMS:
            for subbenchmark in MachSuiteBenchmarks.BENCHMARK_PARAMS[benchmark]:
                name = self.getName(benchmark, subbenchmark)
                raw_bc = self.getRawBitcodeName(benchmark, subbenchmark)
                self.initBenchmark(benchmark, subbenchmark)
                self.benchmarks[name] = Benchmark(
                    name=name, raw_bc=raw_bc, links=['-lm'], args=None, trace_func='run_benchmark')

    def baseline(self, benchmark, subbenchmark):
        path = os.path.join(self.cwd, self.folder, benchmark, subbenchmark)
        os.chdir(path)
        baseline_binary = self.getBaselineName(benchmark, subbenchmark)
        baseline_bc = self.getBaselineBitcodeName(benchmark, subbenchmark)
        self.buildGem5Binary(benchmark=benchmark, cflags=MachSuiteBenchmarks.CFLAGS,
                             output_binary=baseline_binary, output_bc=baseline_bc)
        gem5_outdir = self.gem5Baseline(
            benchmark, subbenchmark, baseline_binary)
        # Copy the result out.
        os.chdir(self.cwd)
        Util.call_helper(['cp', os.path.join(gem5_outdir, 'stats.txt'), os.path.join(
            self.cwd, 'result', self.getName(benchmark, subbenchmark) + '.baseline.txt')])

    def baseline_simd(self, benchmark, subbenchmark):
        path = os.path.join(self.cwd, self.folder, benchmark, subbenchmark)
        os.chdir(path)
        cflags = list(MachSuiteBenchmarks.CFLAGS)
        cflags.remove('-fno-vectorize')
        cflags.append('-DLLVM_TDG_SIMD')
        binary = self.getName(benchmark, subbenchmark) + '.baseline.simd'
        bc = self.getName(benchmark, subbenchmark) + '.baseline.simd.bc'
        self.buildGem5Binary(benchmark=benchmark, cflags=cflags,
                             output_binary=binary, output_bc=bc)
        gem5_outdir = self.gem5Baseline(benchmark, subbenchmark, binary)
        # Copy the result out.
        os.chdir(self.cwd)
        Util.call_helper(['cp', os.path.join(gem5_outdir, 'stats.txt'), os.path.join(
            self.cwd, 'result', self.getName(benchmark, subbenchmark) + '.baseline.simd.txt')])

    def trace(self, benchmark, subbenchmark):
        path = os.path.join(self.cwd, self.folder, benchmark, subbenchmark)
        name = self.getName(benchmark, subbenchmark)
        os.chdir(path)
        self.benchmarks[name].build_trace()
        self.benchmarks[name].run_trace()
        os.chdir(self.cwd)

    def build_replay(self, benchmark, subbenchmark):
        path = os.path.join(self.cwd, self.folder, benchmark, subbenchmark)
        name = self.getName(benchmark, subbenchmark)
        os.chdir(path)
        self.benchmarks[name].build_replay()
        os.chdir(self.cwd)

    def run_replay(self, benchmark, subbenchmark):
        path = os.path.join(self.cwd, self.folder, benchmark, subbenchmark)
        name = self.getName(benchmark, subbenchmark)
        os.chdir(path)
        gem5_outdir = self.benchmarks[name].gem5_replay()
        # Copy the result out.
        os.chdir(self.cwd)
        Util.call_helper(['cp', os.path.join(gem5_outdir, 'stats.txt'), os.path.join(
            self.cwd, 'result', name + '.replay.txt')])

    def run_standalone(self, benchmark, subbenchmark):
        path = os.path.join(self.cwd, self.folder, benchmark, subbenchmark)
        name = self.getName(benchmark, subbenchmark)
        os.chdir(path)
        gem5_outdir = self.benchmarks[name].gem5_replay(standalone=1)
        # Copy the result out.
        os.chdir(self.cwd)
        Util.call_helper(['cp', os.path.join(gem5_outdir, 'stats.txt'), os.path.join(
            self.cwd, 'result', name + '.standalone.txt')])

    """
    Set up the benchmark.
    1. Build the raw bc.
    2. Build the baseline bc for gem5.
    """

    def initBenchmark(self, benchmark, subbenchmark):
        path = os.path.join(self.cwd, self.folder, benchmark, subbenchmark)
        os.chdir(path)
        # Generate the input. Backprop doesnot require generating the input.
        if benchmark != 'backprop':
            Util.call_helper(['make', 'generate'])
            Util.call_helper(['./generate'])
        # Build the raw bitcode.
        self.buildRawBC(benchmark, subbenchmark)
        os.chdir(self.cwd)

    def getName(self, benchmark, subbenchmark):
        return benchmark + '.' + subbenchmark

    def getRawBitcodeName(self, benchmark, subbenchmark):
        return self.getName(benchmark, subbenchmark) + '.bc'

    def getBaselineBitcodeName(self, benchmark, subbenchmark):
        return self.getName(benchmark, subbenchmark) + '.baseline.bc'

    def getBaselineName(self, benchmark, subbenchmark):
        return self.getName(benchmark, subbenchmark) + '.baseline'

    """
    Build the bit code. This is done by first compile every source file 
    into llvm and then link together.
    Paramter
    ========
    benchmark: the name of the benchmark.
    output_bc: the name of the output bc.
    cflags: cflags for this build.

    Returns
    =======
    void
    """

    def buildBC(self, benchmark, output_bc, cflags):

        sources = list(MachSuiteBenchmarks.COMMON_SOURCES)
        sources.append(benchmark + '.c')
        bcs = list()
        for source in sources:
            assert(source[-2:] == '.c')
            bc = source.replace('.c', '.bc')
            compile_cmd = [
                'ecc',
                '-emit-llvm',
                '-o',
                bc,
                '-c',
                '-I{INCLUDE_DIR}'.format(
                    INCLUDE_DIR=MachSuiteBenchmarks.INCLUDE_DIR),
                source,
            ]
            compile_cmd += cflags
            print('# buildBC: Compiling {source}...'.format(source=source))
            # print(' '.join(compile_cmd))
            Util.call_helper(compile_cmd)
            bcs.append(bc)

        link_cmd = [
            'llvm-link',
            '-o',
            output_bc,
        ]
        link_cmd += bcs
        print('# buildBC: Linking...')
        Util.call_helper(link_cmd)
        print('# buildBC: Naming everthing...')
        Util.call_helper(['opt', '-instnamer', output_bc, '-o', output_bc])
        clean_cmd = ['rm'] + bcs
        print('# buildBC: Cleaning...')
        Util.call_helper(clean_cmd)

    """
    Build the raw bit code.
    """

    def buildRawBC(self, benchmark, subbenchmark):
        raw_bc = self.getRawBitcodeName(benchmark, subbenchmark)
        self.buildBC(benchmark=benchmark, output_bc=raw_bc,
                     cflags=MachSuiteBenchmarks.CFLAGS)

    """
    Link a binary from bitcode. This binary can be run directly in gem5.
    This will define LLVM_TDG_GEM5_BASELINE and include some m5op,
    so that we only get statistics of ROI.
    """

    def linkGem5BinaryFromBC(self, input_bc, output_binary):
        GEM5_PSEUDO_S = os.path.join(
            C.GEM5_DIR, 'util', 'm5', 'm5op_x86.S')
        build_cmd = [
            'ecc',
            '-static',
            '-o',
            output_binary,
            '-I{INCLUDE_DIR}'.format(INCLUDE_DIR=C.GEM5_INCLUDE_DIR),
            input_bc,
            GEM5_PSEUDO_S,
            '-lm',
        ]
        print('# Building the gem5 binary...')
        Util.call_helper(build_cmd)

    """
    Build the baseline.
    A baseline bc is different than a raw bc because it use m5ops to 
    communicate with gem5 so that we only collect statistics from 
    ROI.
    """

    def buildGem5Binary(self, benchmark, cflags, output_binary, output_bc):
        # Make a copy of the lists.
        cflags = list(cflags)
        cflags.append('-DLLVM_TDG_GEM5_BASELINE')
        cflags.append('-I{INCLUDE_DIR}'.format(INCLUDE_DIR=C.GEM5_INCLUDE_DIR))

        self.buildBC(benchmark=benchmark, output_bc=output_bc, cflags=cflags)

        # Link the m5op into a binary.
        self.linkGem5BinaryFromBC(output_bc, output_binary)

    """
    Run the baseline program.

    Returns
    -------
    The absolute address to the gem5 output directory.
    """

    def gem5Baseline(self, benchmark, subbenchmark, binary):
        GEM5_OUT_DIR = '{cpu_type}.baseline'.format(cpu_type=C.CPU_TYPE)
        Util.call_helper(['mkdir', '-p', GEM5_OUT_DIR])
        gem5_args = [
            C.GEM5_X86,
            '--outdir={outdir}'.format(outdir=GEM5_OUT_DIR),
            C.GEM5_LLVM_TRACE_SE_CONFIG,
            '--cmd={cmd}'.format(cmd=binary),
            '--llvm-standalone=0',
            '--llvm-issue-width={ISSUE_WIDTH}'.format(
                ISSUE_WIDTH=C.ISSUE_WIDTH),
            '--llvm-store-queue-size={STORE_QUEUE_SIZE}'.format(
                STORE_QUEUE_SIZE=C.STORE_QUEUE_SIZE),
            '--caches',
            '--l2cache',
            '--cpu-type={cpu_type}'.format(cpu_type=C.CPU_TYPE),
            '--num-cpus=1',
            '--l1d_size={l1d_size}'.format(l1d_size=C.GEM5_L1D_SIZE),
            '--l1i_size={l1i_size}'.format(l1i_size=C.GEM5_L1I_SIZE),
            '--l2_size={l2_size}'.format(l2_size=C.GEM5_L2_SIZE),
            # Exit after the work_end
            '--work-end-exit-count=1',
        ]
        print('# Running the baseline...')
        Util.call_helper(gem5_args)
        return os.path.join(os.getcwd(), GEM5_OUT_DIR)

    def draw(self, pdf_fn, baseline, test, names):
        results = Util.Results()
        for name in names:
            results.addResult(baseline, name, os.path.join(
                'result', name + '.' + baseline + '.txt'))
            results.addResult(test, name, os.path.join(
                'result', name + '.' + test + '.txt'))

        variables = [
            Util.Variable(name='Cycle',
                          baseline_key='system.cpu.numCycles',
                          test_key='system.cpu1.numCycles',
                          color=[0, 76, 153]),
            Util.Variable(name='Read',
                          baseline_key='system.cpu.dcache.ReadReq_accesses::total',
                          test_key='system.cpu1.dcache.ReadReq_accesses::total',
                          color=[0, 153, 153]),
            Util.Variable(name='Write',
                          baseline_key='system.cpu.dcache.WriteReq_accesses::total',
                          test_key='system.cpu1.dcache.WriteReq_accesses::total',
                          color=[76, 153, 0]),
            Util.Variable(name='MicroOp',
                          baseline_key='system.cpu.commit.committedOps',
                          test_key='system.cpu1.commit.committedOps',
                          color=[153, 153, 0]),
        ]

        results.draw(pdf_fn, baseline, test, variables)


def main(folder):
    benchmarks = MachSuiteBenchmarks(folder)
    names = list()
    for benchmark in MachSuiteBenchmarks.BENCHMARK_PARAMS:
        for subbenchmark in MachSuiteBenchmarks.BENCHMARK_PARAMS[benchmark]:
            benchmarks.baseline(benchmark, subbenchmark)
            # benchmarks.baseline_simd(benchmark, subbenchmark)
            benchmarks.trace(benchmark, subbenchmark)
            benchmarks.build_replay(benchmark, subbenchmark)
            benchmarks.run_replay(benchmark, subbenchmark)
            benchmarks.run_standalone(benchmark, subbenchmark)
            names.append(benchmarks.getName(benchmark, subbenchmark))

    benchmarks.draw('MachSuite.baseline.replay.pdf',
                    'baseline', 'replay', names)
    benchmarks.draw('MachSuite.standalone.replay.pdf',
                    'standalone', 'replay', names)


if __name__ == '__main__':
    import sys
    if len(sys.argv) == 1:
        # Use the current folder.
        main('.')
    else:
        main(sys.argv[1])
