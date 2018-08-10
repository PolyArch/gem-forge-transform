import os
import subprocess
import multiprocessing

import Constants as C
import Util
import StreamStatistics
from Benchmark import Benchmark


class MachSuiteBenchmark:

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
        '-O3',
        '-Wall',
        '-Wno-unused-label',
        '-fno-inline-functions',
        '-fno-vectorize',
        '-fno-slp-vectorize',
        '-fno-unroll-loops'
    ]

    def __init__(self, folder, benchmark, subbenchmark):
        self.benchmark_name = benchmark
        self.subbenchmark_name = subbenchmark
        self.cwd = os.getcwd()
        self.folder = folder
        self.work_path = os.path.join(
            self.cwd,
            self.folder,
            self.benchmark_name,
            self.subbenchmark_name)
        self.benchmark = Benchmark(
            name=self.get_name(),
            raw_bc=self.get_raw_bc(),
            links=['-lm'],
            args=None,
            trace_func='run_benchmark')

    def get_name(self):
        return 'mach.{b}.{sub}'.format(
            b=self.benchmark_name, sub=self.subbenchmark_name)

    def get_raw_bc(self):
        return '{name}.bc'.format(name=self.get_name())

    def get_baseline_binary(self):
        return '{name}.baseline'.format(name=self.get_name())

    def get_result(self, transform):
        return os.path.join(
            self.cwd,
            'result',
            '{name}.{transform}.txt'.format(name=self.get_name(), transform=transform))

    def get_trace(self):
        return '{name}.trace'.format(name=self.get_name())

    def get_trace_result(self):
        # Guarantee that there is only one trace result.
        return self.get_trace() + '.0'

    def get_profile(self):
        return self.get_trace() + '.profile'

    def get_profile(self):
        return self.get_trace() + '.profile'

    def get_tdg(self, transform):
        return os.path.join(
            self.work_path,
            '{name}.{transform}.tdg'.format(
                name=self.get_name(), transform=transform)
        )

    def get_tdgs(self, transform):
        return [self.get_tdg(transform), ]

    def compile(self, output_bc, defines=[], includes=[]):
        sources = list(MachSuiteBenchmark.COMMON_SOURCES)
        sources.append(self.benchmark_name + '.c')
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
                    INCLUDE_DIR=MachSuiteBenchmark.INCLUDE_DIR),
                source,
            ]
            compile_cmd += MachSuiteBenchmark.CFLAGS
            for define in defines:
                compile_cmd.append('-D{DEFINE}'.format(DEFINE=define))
            for include in includes:
                compile_cmd.append('-I{INCLUDE}'.format(INCLUDE=include))
            print('# compile: Compiling {source}...'.format(source=source))
            # print(' '.join(compile_cmd))
            Util.call_helper(compile_cmd)
            bcs.append(bc)

        link_cmd = [
            'llvm-link',
            '-o',
            output_bc,
        ]
        link_cmd += bcs
        print('# compile: Linking...')
        Util.call_helper(link_cmd)
        print('# compile: Naming everthing...')
        Util.call_helper(['opt', '-instnamer', output_bc, '-o', output_bc])
        clean_cmd = ['rm'] + bcs
        print('# compile: Cleaning...')
        Util.call_helper(clean_cmd)

    def build_raw_bc(self):
        os.chdir(self.work_path)
        # Generate the input. Backprop doesnot require generating the input.
        if self.benchmark_name != 'backprop':
            Util.call_helper(['make', 'generate'])
            Util.call_helper(['./generate'])
        self.compile(self.get_raw_bc())
        os.chdir(self.cwd)

    def build_baseline(self):
        defines = ['LLVM_TDG_GEM5_BASELINE']
        includes = [C.GEM5_INCLUDE_DIR]
        output_bc = 'tmp.bc'
        self.compile(output_bc, defines, includes)
        build_cmd = [
            'ecc',
            '-static',
            '-o',
            self.get_baseline_binary(),
            '-I{INCLUDE_DIR}'.format(INCLUDE_DIR=C.GEM5_INCLUDE_DIR),
            output_bc,
            C.GEM5_M5OPS_X86,
            '-lm',
        ]
        print('# Building the gem5 binary...')
        Util.call_helper(build_cmd)
        Util.call_helper(['rm', output_bc])

    def run_baseline(self):
        GEM5_OUT_DIR = '{cpu_type}.baseline'.format(cpu_type=C.CPU_TYPE)
        Util.call_helper(['mkdir', '-p', GEM5_OUT_DIR])

        binary = self.get_baseline_binary()
        gem5_cmd = [
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
        Util.call_helper(gem5_cmd)
        return os.path.join(os.getcwd(), GEM5_OUT_DIR)

    def baseline(self):

        Util.call_helper(['mkdir', '-p', 'result'])

        os.chdir(self.work_path)
        self.build_baseline()
        gem5_outdir = self.run_baseline()
        # Copy the result out.
        os.chdir(self.cwd)
        Util.call_helper(
            ['cp', os.path.join(gem5_outdir, 'stats.txt'), self.get_result('baseline')])

    def trace(self):
        os.chdir(self.work_path)
        self.benchmark.build_trace()
        self.benchmark.run_trace(self.get_trace())
        os.chdir(self.cwd)

    def transform(self, transform, debugs):
        pass_name = 'replay'
        if transform == 'adfa':
            pass_name = 'abs-data-flow-acc-pass'
        elif transform == 'stream':
            pass_name = 'stream-pass'
        elif transform == 'replay':
            pass_name = 'replay'
        else:
            assert(False)

        os.chdir(self.work_path)

        self.benchmark.build_replay(
            pass_name=pass_name,
            trace_file=self.get_trace_result(),
            profile_file=self.get_profile(),
            tdg_detail='integrated',
            # tdg_detail='standalone',
            output_tdg=self.get_tdg(transform),
            debugs=debugs,
        )

        os.chdir(self.cwd)

    def simulate(self, transform, debugs):
        os.chdir(self.work_path)
        gem5_outdir = self.benchmark.gem5_replay(
            standalone=0,
            output_tdg=self.get_tdg(transform),
            debugs=debugs,
        )
        Util.call_helper([
            'cp',
            # os.path.join(gem5_outdir, 'stats.txt'),
            os.path.join(gem5_outdir, 'region.stats.txt'),
            self.get_result(transform),
        ])
        os.chdir(self.cwd)

    def statistics(self):
        os.chdir(self.work_path)
        debugs = [
            'TraceStatisticPass',
        ]
        self.benchmark.get_trace_statistics(
            trace_file=self.get_trace_result(),
            profile_file=self.get_profile(),
            debugs=debugs,
        )
        os.chdir(self.cwd)


class MachSuiteBenchmarks:

    BENCHMARK_PARAMS = {
        "bfs": [
            "queue",
            #     # "bulk" # not working
        ],
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
        #     # "radix",
        #     "merge"
        # ],
        # "spmv": [
        #     "ellpack",
        #     "crs"
        # ],
        # "kmp": [
        #     "kmp"
        # ],
        # #  "backprop": [
        # #     "backprop" # Not working.
        # #  ],
        # "gemm": [
        #     "blocked",
        #     "ncubed"
        # ],
        # "nw": [
        #     "nw"
        # ]
    }

    def __init__(self, folder):
        self.folder = folder
        self.cwd = os.getcwd()

        Util.call_helper(['mkdir', '-p', 'result'])

        self.benchmarks = dict()
        for benchmark in MachSuiteBenchmarks.BENCHMARK_PARAMS:
            self.benchmarks[benchmark] = dict()
            for subbenchmark in MachSuiteBenchmarks.BENCHMARK_PARAMS[benchmark]:
                self.benchmarks[benchmark][subbenchmark] = MachSuiteBenchmark(
                    folder, benchmark, subbenchmark)

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


def run_benchmark(benchmark):
    print('start run benchmark ' + benchmark.get_name())
    # benchmark.baseline()
    # benchmark.build_raw_bc()
    # benchmark.trace()
    # benchmark.statistics()

    # Basic replay.
    # debugs = [
    #     'ReplayPass',
    #     # 'DataGraph',
    #     # 'TDGSerializer'
    # ]
    # benchmark.transform('replay', debugs)
    # debugs = [
    #     # 'LLVMTraceCPU',
    #     'RegionStats',
    # ]
    # benchmark.simulate('replay', debugs)

    # Abstract data flow replay.
    # debugs = [
    #     'ReplayPass',
    #     'DynamicInstruction',
    #     'TDGSerializer',
    #     # 'AbstractDataFlowAcceleratorPass',
    #     # 'LoopUtils',
    # ]
    # benchmark.transform('adfa', debugs)
    # debugs = [
    #     # 'AbstractDataFlowAccelerator',
    #     # 'LLVMTraceCPU',
    #     'RegionStats',
    # ]
    # benchmark.simulate('adfa', debugs)

    # Stream.
    debugs = [
        'ReplayPass',
        'StreamPass',
        # 'MemoryAccessPattern',
    ]
    benchmark.transform('stream', debugs)
    # debugs = [
    #     # 'LLVMTraceCPU',
    # ]
    # benchmark.simulate('stream', debugs)


def main(folder):
    benchmarks = MachSuiteBenchmarks(folder)
    names = list()
    processes = list()
    bs = list()
    for benchmark_name in MachSuiteBenchmarks.BENCHMARK_PARAMS:
        for subbenchmark_name in MachSuiteBenchmarks.BENCHMARK_PARAMS[benchmark_name]:
            benchmark = benchmarks.benchmarks[benchmark_name][subbenchmark_name]
            processes.append(multiprocessing.Process(
                target=run_benchmark, args=(benchmark,)))
            names.append(benchmark.get_name())
            bs.append(benchmark)

    # We start 4 processes each time until we are done.
    prev = 0
    issue_width = 8
    for i in xrange(len(processes)):
        processes[i].start()
        if (i + 1) % issue_width == 0:
            while prev < i:
                processes[prev].join()
                prev += 1
    while prev < len(processes):
        processes[prev].join()
        prev += 1

    for benchmark in bs:
        tdgs = benchmark.get_tdgs('stream')
        tdg_stats = [tdg + '.stats.txt' for tdg in tdgs]
        stream_stats = StreamStatistics.StreamStatistics(tdg_stats)
        print('-------------------------- ' + benchmark.get_name())
        stream_stats.print_stats()
        stream_stats.print_access()

    # Util.ADFAAnalyzer.analyze_adfa(bs)

    # benchmarks.draw('MachSuite.baseline.replay.pdf',
    #                 'baseline', 'replay', names)
    # benchmarks.draw('MachSuite.standalone.replay.pdf',
    #                 'standalone', 'replay', names)


if __name__ == '__main__':
    import sys
    if len(sys.argv) == 1:
        # Use the current folder.
        main('.')
    else:
        main(sys.argv[1])
