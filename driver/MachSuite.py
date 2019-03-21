import os
import subprocess
import multiprocessing
import prettytable

import Constants as C
import Util
import StreamStatistics
from Benchmark import Benchmark
from Utils import McPAT


class MachSuiteBenchmark(Benchmark):

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
        super(MachSuiteBenchmark, self).__init__(
            name=self.get_name(),
            raw_bc=self.get_raw_bc(),
            links=['-lm'],
            args=None,
            trace_func='run_benchmark',
            standalone=1)

    def get_name(self):
        return 'mach.{b}.{sub}'.format(
            b=self.benchmark_name, sub=self.subbenchmark_name)

    def get_baseline_result(self):
        return os.path.join(
            C.LLVM_TDG_RESULT_DIR,
            self.get_name() + '.baseline.txt'
        )

    def get_raw_bc(self):
        return '{name}.bc'.format(name=self.get_name())

    def get_trace_ids(self):
        return [0]

    def get_run_path(self):
        return self.work_path

    def get_baseline_binary(self):
        return '{name}.baseline'.format(name=self.get_name())

    def compile(self, output_bc, defines=[], includes=[]):
        sources = list(MachSuiteBenchmark.COMMON_SOURCES)
        sources.append(self.benchmark_name + '.c')
        bcs = list()
        for source in sources:
            assert(source[-2:] == '.c')
            bc = source.replace('.c', '.bc')
            compile_cmd = [
                C.CC,
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
            C.LLVM_LINK,
            '-o',
            output_bc,
        ]
        link_cmd += bcs
        print('# compile: Linking...')
        Util.call_helper(link_cmd)
        print('# compile: Naming everthing...')
        Util.call_helper([C.OPT, '-instnamer', output_bc, '-o', output_bc])
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
            C.CC,
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
            '--llvm-mcpat=1',
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
        os.chdir(self.work_path)
        self.build_baseline()
        gem5_outdir = self.run_baseline()
        # Copy the result out.
        os.chdir(self.cwd)
        Util.call_helper(
            ['cp', os.path.join(gem5_outdir, 'stats.txt'), self.get_baseline_result()])
        return gem5_outdir

    def trace(self):
        os.chdir(self.work_path)
        self.build_trace()
        self.run_trace(self.get_name())
        os.chdir(self.cwd)

    def transform(self, pass_name, trace, profile_file, tdg, debugs):
        os.chdir(self.work_path)

        self.build_replay(
            pass_name=pass_name,
            trace=trace,
            profile_file=profile_file,
            tdg_detail='integrated',
            # tdg_detail='standalone',
            output_tdg=tdg,
            debugs=debugs,
        )

        os.chdir(self.cwd)


class MachSuiteBenchmarks:

    BENCHMARK_PARAMS = {
        "bfs": [
            "queue",
            #     # "bulk" # not working
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
            # "radix",
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
        #     # "backprop" # Not working.
        #  ],
        "gemm": [
            "blocked",
            "ncubed"
        ],
        "nw": [
            "nw"
        ]
    }

    def __init__(self, folder):

        self.benchmarks = dict()
        for benchmark in MachSuiteBenchmarks.BENCHMARK_PARAMS:
            self.benchmarks[benchmark] = dict()
            for subbenchmark in MachSuiteBenchmarks.BENCHMARK_PARAMS[benchmark]:
                self.benchmarks[benchmark][subbenchmark] = MachSuiteBenchmark(
                    folder, benchmark, subbenchmark)

    def get_benchmarks(self):
        results = list()
        for benchmark_name in MachSuiteBenchmarks.BENCHMARK_PARAMS:
            for subbenchmark_name in MachSuiteBenchmarks.BENCHMARK_PARAMS[benchmark_name]:
                benchmark = self.benchmarks[benchmark_name][subbenchmark_name]
                results.append(benchmark)
        return results

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

    # Util.ADFAAnalyzer.analyze_adfa(bs)

    # benchmarks.draw('MachSuite.baseline.replay.pdf',
    #                 'baseline', 'replay', names)
    # benchmarks.draw('MachSuite.standalone.replay.pdf',
    #                 'standalone', 'replay', names)


def show_difference(all_mcpats):
    baseline_sums = dict()
    replay_sums = dict()
    for b in all_mcpats:
        baseline_mcpat, replay_mcpat = all_mcpats[b]
        for component in baseline_mcpat.components:
            for idx in xrange(len(baseline_mcpat.components[component])):
                baseline_c = baseline_mcpat.components[component][idx]
                replay_c = replay_mcpat.components[component][idx]
                name = baseline_c.get_name()
                if name not in baseline_sums:
                    baseline_sums[name] = 0.0
                    replay_sums[name] = 0.0
                baseline_sums[name] += baseline_c.dynamic_power
                replay_sums[name] += replay_c.dynamic_power
    table = prettytable.PrettyTable([
        'component',
        'baseline',
        'replay',
        'diff',
    ])
    for name in baseline_sums:
        diff = '/'
        if baseline_sums[name] != 0.0:
            diff = (replay_sums[name] - baseline_sums[name]
                    ) / baseline_sums[name] * 100
        row = [
            name,
            baseline_sums[name],
            replay_sums[name],
            diff,
        ]
        table.add_row(row)
    table.float_format = '.2'
    print(table)


if __name__ == '__main__':
    # Main function will run the baseline.
    suite = MachSuiteBenchmarks('./MachSuite')
    benchmarks = suite.get_benchmarks()

    sum_table = prettytable.PrettyTable([
        'benchmark',
        'baseline',
        'replay',
    ])

    baseline_replay_mcpats = dict()
    replay_stream_mcpats = dict()

    for benchmark in benchmarks:
        # benchmark.baseline()

        table = prettytable.PrettyTable([
            'component',
            'baseline',
            'replay',
        ])
        baseline_gem5_dir = os.path.join(
            benchmark.get_run_path(),
            '{cpu_type}.baseline'.format(cpu_type=C.CPU_TYPE))
        baseline_mcpat = McPAT.McPAT(
            os.path.join(baseline_gem5_dir, 'mcpat.txt'))

        # Only one tdg for MachSuite.
        replay_tdg = benchmark.get_tdgs('replay')[0]
        replay_gem5_dir = os.path.join(
            benchmark.get_run_path(),
            benchmark.gem5_config.get_gem5_dir(replay_tdg)
        )
        replay_mcpat = McPAT.McPAT(os.path.join(replay_gem5_dir, 'mcpat.txt'))

        stream_tdg = benchmark.get_tdgs('stream')[0]
        stream_gem5_dir = os.path.join(
            benchmark.get_run_path(),
            benchmark.gem5_config.get_gem5_dir(stream_tdg)
        )
        stream_mcpat = McPAT.McPAT(os.path.join(stream_gem5_dir, 'mcpat.txt'))

        for component in baseline_mcpat.components:
            baseline_components = baseline_mcpat.components[component]
            replay_components = replay_mcpat.components[component]
            assert(len(baseline_components) == len(replay_components))
            for i in xrange(len(baseline_components)):
                row = [
                    baseline_components[i].get_name(),
                    baseline_components[i].dynamic_power,
                    replay_components[i].dynamic_power,
                ]
                table.add_row(row)
        print(table.get_string(title=benchmark.get_name()))
        row = [
            benchmark.get_name(),
            baseline_mcpat.components['core'][0].dynamic_power,
            replay_mcpat.components['core'][0].dynamic_power,
        ]
        sum_table.add_row(row)

        baseline_replay_mcpats[benchmark.get_name()] = (
            baseline_mcpat, replay_mcpat)
        replay_stream_mcpats[benchmark.get_name()] = (
            replay_mcpat, stream_mcpat)

    print(sum_table.get_string(title='sum'))
    show_difference(baseline_replay_mcpats)
    show_difference(replay_stream_mcpats)
