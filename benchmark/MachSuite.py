import os
import subprocess

import Constants as C
import Util
from Benchmark import Benchmark


class MachSuiteBenchmarks:

    BENCHMARK_PARAMS = {
        # "bfs": [
        #     "queue",
        #     "bulk"
        # ],
        # "aes": [
        #     "aes"
        # ],
        # "stencil": [
        #     "stencil2d",
        #     "stencil3d"
        # ],
        # "md": [
        #     "grid",
        #     "knn"
        # ],
        # "fft": [
        #     "strided",
        #     "transpose"
        # ],
        # "viterbi": [
        #     "viterbi"
        # ],
        # "sort": [
        #     "radix",
        #     "merge"
        # ],
        # "spmv": [
        #     "ellpack",
        #     "crs"
        # ],
        # "kmp": [
        #     "kmp"
        # ],
        "backprop": [
            "backprop"
        ],
        # "gemm": [
        #     "blocked",
        #     "ncubed"
        # ],
        # "nw": [
        #     "nw"
        # ]
    }

    COMMON_SOURCES = [
        'local_support.c',
        '../../common/support.c',
        '../../common/harness.c',
    ]

    INCLUDE_DIR = '../../common'

    CFLAGS = ['-O3', '-Wall',
              '-Wno-unused-label', '-fno-inline-functions']

    def __init__(self, folder):
        self.folder = folder
        self.cwd = os.getcwd()

        subprocess.check_call(['mkdir', '-p', 'result'])

        self.benchmarks = dict()
        for benchmark in MachSuiteBenchmarks.BENCHMARK_PARAMS:
            for subbenchmark in MachSuiteBenchmarks.BENCHMARK_PARAMS[benchmark]:
                name = self.getName(benchmark, subbenchmark)
                raw_bc = self.getRawBitcodeName(benchmark, subbenchmark)
                # self.initBenchmark(benchmark, subbenchmark)
                self.benchmarks[name] = Benchmark(
                    name=name, raw_bc=raw_bc, links=['-lm'], args=None, trace_func='run_benchmark')

    def baseline(self, benchmark, subbenchmark):
        path = os.path.join(self.cwd, self.folder, benchmark, subbenchmark)
        os.chdir(path)
        self.buildBaseline(benchmark, subbenchmark)
        gem5_outdir = self.gem5Baseline(benchmark, subbenchmark)
        # Copy the result out.
        os.chdir(self.cwd)
        subprocess.check_call(['cp', os.path.join(gem5_outdir, 'stats.txt'), os.path.join(
            self.cwd, 'result', self.getName(benchmark, subbenchmark) + '.baseline.txt')])

    def trace(self, benchmark, subbenchmark):
        path = os.path.join(self.cwd, self.folder, benchmark, subbenchmark)
        name = self.getName(benchmark, subbenchmark)
        os.chdir(path)
        self.benchmarks[name].trace()
        os.chdir(self.cwd)

    def replay(self, benchmark, subbenchmark):
        path = os.path.join(self.cwd, self.folder, benchmark, subbenchmark)
        name = self.getName(benchmark, subbenchmark)
        os.chdir(path)
        self.benchmarks[name].build_replay()
        gem5_outdir = self.benchmarks[name].gem5_replay()
        # Copy the result out.
        os.chdir(self.cwd)
        subprocess.check_call(['cp', os.path.join(gem5_outdir, 'stats.txt'), os.path.join(
            self.cwd, 'result', name + '.replay.txt')])

    """
    Set up the benchmark.
    """

    def initBenchmark(self, benchmark, subbenchmark):
        path = os.path.join(self.cwd, self.folder, benchmark, subbenchmark)
        os.chdir(path)
        # Generate the input. Backprop doesnot require generating the input.
        if benchmark != 'backprop':
            subprocess.check_call(['make', 'generate'])
            subprocess.check_call(['./generate'])
        # Build the raw bitcode.
        self.buildRaw(benchmark, subbenchmark)
        os.chdir(self.cwd)

    def getName(self, benchmark, subbenchmark):
        return benchmark + '.' + subbenchmark

    def getRawBitcodeName(self, benchmark, subbenchmark):
        return self.getName(benchmark, subbenchmark) + '.bc'

    def getBaselineName(self, benchmark, subbenchmark):
        return self.getName(benchmark, subbenchmark) + '.baseline'

    def buildRaw(self, benchmark, subbenchmark):
        raw_bc = self.getRawBitcodeName(benchmark, subbenchmark)
        build_cmd = [
            'clang',
            '-I{INCLUDE_DIR}'.format(INCLUDE_DIR=MachSuiteBenchmarks.INCLUDE_DIR),
            '-flto',
            '-Wl,-plugin-opt=emit-llvm',
        ]
        build_cmd += MachSuiteBenchmarks.CFLAGS
        build_cmd += MachSuiteBenchmarks.COMMON_SOURCES
        build_cmd.append(benchmark + '.c')
        build_cmd += ['-o', raw_bc]
        print('# Building raw llvm bitcode...')
        subprocess.check_call(build_cmd)
        print('# Naming everything in the llvm bitcode...')
        subprocess.check_call(['opt', '-instnamer', raw_bc, '-o', raw_bc])

    """
    Build the baseline binary for gem5.
    This will define LLVM_TDG_BASELINE and enable the m5_work_begin/m5_work_end 
    pseudo instructions.
    """

    def buildBaseline(self, benchmark, subbenchmark):
        name = self.getName(benchmark, subbenchmark)
        baseline_bc = name + '.baseline.bc'
        GEM5_PSEUDO_S = os.path.join(
            C.GEM5_DIR, 'util', 'm5', 'm5op_x86.S')
        build_cmd = [
            'clang',
            '-DLLVM_TDG_GEM5_BASELINE',
            '-o',
            baseline_bc,
            '-I{INCLUDE_DIR}'.format(INCLUDE_DIR=MachSuiteBenchmarks.INCLUDE_DIR),
            '-I{INCLUDE_DIR}'.format(INCLUDE_DIR=C.GEM5_INCLUDE_DIR),
            '-flto',
            '-Wl,-plugin-opt=emit-llvm',
        ]
        build_cmd += MachSuiteBenchmarks.CFLAGS
        build_cmd += MachSuiteBenchmarks.COMMON_SOURCES
        build_cmd.append(benchmark + '.c')
        print('# Compiling baseline llvm bitcode...')
        subprocess.check_call(build_cmd)
        # Compile to assembly.
        baseline_o = name + '.baseline.o'
        compile_cmd = [
            'clang',
            '-O0',
            '-c',
            baseline_bc,
            '-o',
            baseline_o,
        ]
        print('# Compiling baseline object file...')
        subprocess.check_call(compile_cmd)
        # Link to baseline binary.
        link_cmd = [
            'gcc',
            '-O0',
            '-I{INCLUDE_DIR}'.format(INCLUDE_DIR=C.GEM5_INCLUDE_DIR),
            GEM5_PSEUDO_S,
            baseline_o,
            '-o',
            self.getBaselineName(benchmark, subbenchmark),
            '-lm'
        ]
        print('# Linking the baseline binary...')
        subprocess.check_call(link_cmd)

    """
    Run the baseline program.

    Returns
    -------
    The absolute address to the gem5 output directory.
    """

    def gem5Baseline(self, benchmark, subbenchmark):
        GEM5_OUT_DIR = '{cpu_type}.baseline'.format(cpu_type=C.CPU_TYPE)
        subprocess.check_call(['mkdir', '-p', GEM5_OUT_DIR])
        gem5_args = [
            C.GEM5_X86,
            '--outdir={outdir}'.format(outdir=GEM5_OUT_DIR),
            C.GEM5_LLVM_TRACE_SE_CONFIG,
            '--cmd={cmd}'.format(cmd=self.getBaselineName(benchmark,
                                                          subbenchmark)),
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
        subprocess.check_call(gem5_args)
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
            # benchmarks.baseline(benchmark, subbenchmark)
            # benchmarks.trace(benchmark, subbenchmark)
            benchmarks.replay(benchmark, subbenchmark)
            names.append(benchmarks.getName(benchmark, subbenchmark))

    benchmarks.draw('MachSuite.pdf', 'baseline', 'replay', names)


if __name__ == '__main__':
    import sys
    if len(sys.argv) == 1:
        # Use the current folder.
        main('.')
    else:
        main(sys.argv[1])
