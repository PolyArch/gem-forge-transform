import os
import subprocess
import multiprocessing

import Constants as C
import Util
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

    def get_baseline_result(self):
        return os.path.join(
            self.cwd,
            'result',
            '{name}.baseline.txt'.format(name=self.get_name()))

    def get_replay_result(self):
        return os.path.join(
            self.cwd,
            'result',
            '{name}.replay.txt'.format(name=self.get_name()))

    def get_adfa_result(self):
        return os.path.join(
            self.cwd,
            'result',
            '{name}.adfa.txt'.format(name=self.get_name()))

    def get_trace(self):
        return '{name}.trace'.format(name=self.get_name())

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
            ['cp', os.path.join(gem5_outdir, 'stats.txt'), self.get_baseline_result()])

    def trace(self):
        os.chdir(self.work_path)
        self.benchmark.build_trace()
        self.benchmark.run_trace(self.get_trace())
        os.chdir(self.cwd)

    def build_replay(self):
        pass_name = 'replay'
        debugs = [
            'ReplayPass',
            # 'TDGSerializer'
        ]
        self.benchmark.build_replay(
            pass_name=pass_name,
            trace_file=self.get_trace(),
            tdg_detail='integrated',
            # tdg_detail='standalone',
            debugs=debugs,
        )

    def build_replay_abs_data_flow(self):
        pass_name = 'abs-data-flow-acc-pass'
        debugs = [
            'ReplayPass',
            # 'TDGSerializer',
            'AbstractDataFlowAcceleratorPass',
            # 'LoopUtils',
        ]
        self.benchmark.build_replay(
            pass_name=pass_name,
            trace_file=self.get_trace(),
            tdg_detail='integrated',
            # tdg_detail='standalone',
            debugs=debugs,
        )

    def run_replay(self, debugs):
        return self.benchmark.gem5_replay(
            standalone=0,
            debugs=debugs)

    def replay(self):
        os.chdir(self.work_path)
        # Basic replay.
        # self.build_replay()
        # debugs = []
        # gem5_outdir = self.run_replay(debugs=debugs)
        # Util.call_helper([
        #     'cp',
        #     os.path.join(gem5_outdir, 'stats.txt'),
        #     self.get_replay_result(),
        # ])
        # Abstract data flow replay.
        self.build_replay_abs_data_flow()
        debugs = [
            # 'AbstractDataFlowAccelerator'
        ]
        gem5_outdir = self.run_replay(debugs=debugs)
        Util.call_helper([
            'cp',
            os.path.join(gem5_outdir, 'stats.txt'),
            self.get_adfa_result(),
        ])
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
        # #  "backprop": [
        # #     "backprop" # Not working.
        # #  ],
        "gemm": [
            "blocked",
            "ncubed"
        ],
        "nw": [
            "nw"
        ]
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


class ADFAAnalyzer:

    SYS_CPU_PREFIX = 'system.cpu1.'

    @staticmethod
    def compute_speedup(baseline, adfa):
        baselineCycles = baseline[ADFAAnalyzer.SYS_CPU_PREFIX + 'numCycles']
        adfaCycles = adfa[ADFAAnalyzer.SYS_CPU_PREFIX + 'numCycles']
        return baselineCycles / adfaCycles

    @staticmethod
    def compute_runtime_ratio(adfa):
        cpu_cycles = adfa[ADFAAnalyzer.SYS_CPU_PREFIX + 'numCycles']
        adfa_cycles = adfa.get_default('.adfa.numCycles', 0.0)
        return adfa_cycles / cpu_cycles

    @staticmethod
    def analyze_adfa(benchmarks):
        names = list()
        speedup = list()
        num_configs = list()
        num_committed_inst = list()
        num_df = list()
        runtime_ratio = list()
        avg_df_len = list()
        avg_issue = list()
        baseline_issue = list()
        branches = list()
        branch_misses = list()
        cache_misses = list()
        for benchmark_name in benchmarks.benchmarks:
            for subbenchmark_name in benchmarks.benchmarks[benchmark_name]:
                b = benchmarks.benchmarks[benchmark_name][subbenchmark_name]
                name = b.get_name()
                baseline = Util.Gem5Stats(name, b.get_replay_result())
                adfa = Util.Gem5Stats(name, b.get_adfa_result())

                print(name)
                adfa_configs = adfa.get_default('.adfa.numConfigured', 0.0)
                adfa_df = adfa.get_default('.adfa.numExecution', 0.0)
                adfa_insts = adfa.get_default('.adfa.numCommittedInst', 0.0)
                adfa_cycles = adfa.get_default('.adfa.numCycles', 0.0)

                names.append(name)
                speedup.append(ADFAAnalyzer.compute_speedup(baseline, adfa))
                num_configs.append(adfa_configs)
                num_df.append(adfa_df)
                num_committed_inst.append(adfa_insts)
                runtime_ratio.append(ADFAAnalyzer.compute_runtime_ratio(adfa))

                if adfa_insts > 0.0:
                    avg_df_len.append(adfa_insts / adfa_df)
                    avg_issue.append(adfa['.adfa.issued_per_cycle::mean'])
                else:
                    avg_df_len.append(0.0)
                    avg_issue.append(0.0)

                baseline_issue.append(
                    baseline[ADFAAnalyzer.SYS_CPU_PREFIX + 'iew.issued_per_cycle::mean'])

                n_branch = baseline[ADFAAnalyzer.SYS_CPU_PREFIX +
                                    'fetch.branchInsts']
                n_branch_misses = baseline[ADFAAnalyzer.SYS_CPU_PREFIX +
                                           'fetch.branchPredMisses']
                n_insts = baseline[ADFAAnalyzer.SYS_CPU_PREFIX +
                                   'commit.committedInsts']
                n_cache_misses = baseline[ADFAAnalyzer.SYS_CPU_PREFIX +
                                          'dcache.demand_misses::cpu1.data']
                branches.append(n_branch / n_insts * 1000.0)
                branch_misses.append(n_branch_misses / n_insts * 1000.0)
                cache_misses.append(n_cache_misses / n_insts * 1000.0)

        title = (
            '{name:>25} '
            '{speedup:>10} '
            '{configs:>10} '
            '{dfs:>10} '
            '{ratio:>10} '
            '{avg_df_len:>10} '
            '{avg_issue:>10} '
            '{base_issue:>10} '
            '{bpki:>5} '
            '{bmpki:>5} '
            '{cmpki:>5} '
        )
        print(title.format(
            name='benchmark',
            speedup='speedup',
            configs='configs',
            dfs='dataflows',
            ratio='ratio',
            avg_df_len='avg_df_len',
            avg_issue='avg_issue',
            base_issue='base_issue',
            bpki='BPKI',
            bmpki='BMPKI',
            cmpki='CMPKI',
        ))
        line = (
            '{name:>25} '
            '{speedup:>10.2f} '
            '{configs:>10} '
            '{dfs:>10} '
            '{ratio:>10.4f} '
            '{avg_df_len:>10.1f} '
            '{avg_issue:>10.4f} '
            '{base_issue:>10.4f} '
            '{bpki:>5.1f} '
            '{bmpki:>5.1f} '
            '{cmpki:>5.1f} '
        )
        for i in xrange(len(names)):
            print(line.format(
                name=names[i],
                speedup=speedup[i],
                configs=num_configs[i],
                dfs=num_df[i],
                ratio=runtime_ratio[i],
                avg_df_len=avg_df_len[i],
                avg_issue=avg_issue[i],
                base_issue=baseline_issue[i],
                bpki=branches[i],
                bmpki=branch_misses[i],
                cmpki=cache_misses[i],
            ))


def run_benchmark(benchmark):
    print('start run benchmark ' + benchmark.get_name())
    # benchmark.baseline()
    # benchmark.build_raw_bc()
    # benchmark.trace()
    benchmark.replay()


def main(folder):
    benchmarks = MachSuiteBenchmarks(folder)
    names = list()
    processes = list()
    for benchmark_name in MachSuiteBenchmarks.BENCHMARK_PARAMS:
        for subbenchmark_name in MachSuiteBenchmarks.BENCHMARK_PARAMS[benchmark_name]:
            benchmark = benchmarks.benchmarks[benchmark_name][subbenchmark_name]
            processes.append(multiprocessing.Process(
                target=run_benchmark, args=(benchmark,)))
            names.append(benchmark.get_name())

    # We start 4 processes each time until we are done.
    prev = 0
    issue_width = 4
    for i in xrange(len(processes)):
        processes[i].start()
        if (i + 1) % issue_width == 0:
            while prev < i:
                processes[prev].join()
                prev += 1
    while prev < len(processes):
        processes[prev].join()
        prev += 1

    ADFAAnalyzer.analyze_adfa(benchmarks)

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
