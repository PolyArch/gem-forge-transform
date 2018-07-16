import os
import sys
import subprocess

import Constants as C
import Util
from Benchmark import Benchmark


class SPEC2017Benchmark:

    def __init__(self, force_rebuild, **params):

        self.spec = os.environ.get('SPEC')
        if self.spec is None:
            print(
                'Unable to find SPEC env var. Please source shrc before using this script')
            sys.exit()

        self.name = params['name']
        self.target = params['target']
        self.max_insts = params['max_insts']
        self.trace_func = params['trace_func']
        self.skip_inst = params['skip_inst']
        self.trace_stdlib = False
        self.cwd = os.getcwd()

        if force_rebuild:
            # Clear the build/run path.
            clear_cmd = [
                'rm',
                '-rf',
                self.get_build_path()
            ]
            Util.call_helper(clear_cmd)
            clear_cmd = [
                'rm',
                '-rf',
                self.get_run_path()
            ]
            Util.call_helper(clear_cmd)
        # Check if build dir and run dir exists.
        if not (os.path.isdir(self.get_build_path()) and os.path.isdir(self.get_run_path())):
            self.build_raw_bc()

        # Finally use specinvoke to get the arguments.
        os.chdir(self.get_run_path())
        args = self.get_args(subprocess.check_output([
            'specinvoke',
            '-n'
        ]))
        print('{name} has arguments {args}'.format(name=self.name, args=args))
        os.chdir(self.cwd)

        # Initialize the benchmark.
        self.benchmark = Benchmark(
            name=self.name,
            raw_bc=self.get_raw_bc(),
            links=params['links'],
            args=args,
            trace_func=self.trace_func,
            skip_inst=self.skip_inst,
        )

    def get_name(self):
        return self.name

    def get_build_path(self):
        return os.path.join(
            self.spec,
            'benchspec',
            'CPU',
            self.name,
            'build',
            'build_base_LLVM_TDG-m64.0000'
        )

    def get_run_path(self):
        return os.path.join(
            self.spec,
            'benchspec',
            'CPU',
            self.name,
            'run',
            'run_base_refspeed_LLVM_TDG-m64.0000'
        )

    def get_raw_bc(self):
        return self.target + '.bc'

    def get_trace(self):
        return self.target + '.trace'

    def get_replay_result(self):
        return os.path.join(
            self.cwd,
            'result',
            'spec.{name}.replay.txt'.format(name=self.name),
        )

    def get_adfa_result(self):
        return os.path.join(
            self.cwd,
            'result',
            'spec.{name}.adfa.txt'.format(name=self.name),
        )

    def build_raw_bc(self):
        # Fake the runcpu.
        fake_cmd = [
            'runcpu',
            '--fake',
            '--config=ecc-llvm-linux-x86_64',
            '--tune=base',
            name
        ]
        Util.call_helper(fake_cmd)
        # Actually build it.
        os.chdir(self.get_build_path(name))
        try:
            # First try build without target.
            Util.call_helper(['specmake'])
        except subprocess.CalledProcessError as e:
            # Something goes wrong if there are multiple targets,
            # try build with target explicitly specified.
            build_cmd = [
                'specmake',
                'TARGET={target}'.format(target=self.target)
            ]
            Util.call_helper(build_cmd)

        # If we are going to trace the stdlib.
        raw_bc = self.get_raw_bc()
        # Extract the bitcode from the binary.
        binary = self.target
        # Special rule for gcc_s, whose binary is sgcc
        if target == 'gcc_s':
            binary = 'sgcc'
        extract_cmd = [
            'get-bc',
            '-o',
            raw_bc,
            binary
        ]
        print('# Extracting bitcode from binary...')
        Util.call_helper(extract_cmd)
        if self.trace_stdlib:
            # Link with the llvm bitcode of standard library.
            link_cmd = [
                'llvm-link',
                raw_bc,
                C.MUSL_LIBC_LLVM_BC,
                '-o',
                raw_bc
            ]
            print('# Link with stdlib...')
            Util.call_helper(link_cmd)

        # Name everything in the bitcode.
        print('# Naming everything in the llvm bitcode...')
        Util.call_helper(['opt', '-instnamer', raw_bc, '-o', raw_bc])
        # Copy it to run directory.
        Util.call_helper([
            'cp',
            raw_bc,
            self.get_run_path()
        ])
        os.chdir(self.cwd)

    def get_args(self, specinvoke):
        """
        Parse the result of 'specinvoke' command and get the command line arguments.
        """
        for line in specinvoke.split('\n'):
            if len(line) == 0:
                continue
            if line[0] == '#':
                continue
            # Found one.
            fields = line.split()
            assert(len(fields) > 6)
            # Ignore the first one and redirect one.
            # Also ignore other invoke for now.
            return fields[1:-5]

    def trace(self):
        os.chdir(self.get_run_path())
        self.benchmark.build_trace()
        # set the maximum number of insts.
        if self.max_insts != -1:
            os.putenv('LLVM_TDG_MAXIMUM_INST', str(self.max_insts))
        else:
            os.unsetenv('LLVM_TDG_MAXIMUM_INST')
        self.benchmark.run_trace(self.get_trace())
        os.chdir(self.cwd)

    def build_replay(self):
        pass_name = 'replay'
        debugs = [
            'ReplayPass',
        ]
        self.benchmark.build_replay(
            pass_name=pass_name,
            trace_file=self.get_trace(),
            tdg_detail='standalone',
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
            tdg_detail='standalone',
            debugs=debugs,
        )

    def run_replay(self, debugs):
        return self.benchmark.gem5_replay(
            standalone=1,
            debugs=debugs,
        )

    def replay(self):
        os.chdir(self.get_run_path())
        # Basic replay.
        self.build_replay()
        # debugs = []
        # gem5_outdir = self.run_replay(debugs=debugs)
        # Util.call_helper([
        #     'cp',
        #     os.path.join(gem5_outdir, 'stats.txt'),
        #     self.get_replay_result(),
        # ])
        # Abstract data flow replay.
        # self.build_replay_abs_data_flow()
        # debugs = [
        #     # 'LLVMTraceCPU',
        #     'AbstractDataFlowAccelerator'
        # ]
        # gem5_outdir = self.run_replay(debugs=debugs)
        # Util.call_helper([
        #     'cp',
        #     os.path.join(gem5_outdir, 'stats.txt'),
        #     self.get_adfa_result(),
        # ])
        os.chdir(self.cwd)


class SPEC2017Benchmarks:

    BENCHMARK_PARAMS = {
        'lbm_s': {
            'name': '619.lbm_s',
            'links': ['-lm'],
            'skip_inst': 0,
            'max_insts': 100000000,
            'trace_func': 'LBM_performStreamCollideTRT'
        },
        # 'imagick_s': {
        #     'name': '638.imagick_s',
        #     'links': ['-lm'],
        #     'skip_inst': 0,
        #     'max_insts': 100000000,
        #     'trace_func': 'MagickCommandGenesis',
        # },
        # 'nab_s': {
        #     'name': '644.nab_s',
        #     'links': ['-lm'],
        #     'skip_inst': 0,
        #     'max_insts': 100000000,
        #     'trace_func': 'md',
        # },
        # 'x264_s': {
        #     'name': '625.x264_s',
        #     'links': [],
        #     'skip_inst': 100000000,
        #     'max_insts': 100000000,
        #     'trace_func': 'x264_encoder_encode',
        # },
        # 'mcf_s': {
        #     'name': '605.mcf_s',
        #     'links': [],
        #     'skip_inst': 0,
        #     'max_insts': 100000000,
        #     'trace_func': 'global_opt',
        # },
        # 'xz_s': {
        #     'name': '657.xz_s',
        #     'links': [],
        #     'skip_inst': 0,
        #     'max_insts': 100000000,
        #     'trace_func': 'spec_compress,spec_uncompress',
        # },
        # 'gcc_s': {
        #     'name': '602.gcc_s',
        #     'links': [],
        #     'skip_inst': 100000000,
        #     'max_insts': 100000000,
        #     'trace_func': '',
        # },
        # # C++ Benchmark: Notice that SPEC is compiled without
        # # debug information, and we have to use mangled name
        # # for trace_func.
        # 'deepsjeng_s': {
        #     'name': '631.deepsjeng_s',
        #     'links': [],
        #     'skip_inst': 100000000,
        #     'max_insts': 100000000,
        #     'trace_func': '',
        # },
        # 'leela_s': {
        #     'name': '641.leela_s',
        #     'links': [],
        #     'skip_inst': 0,
        #     'max_insts': 100000000,
        #     'trace_func': '_ZN9UCTSearch5thinkEii', # Search::think
        # },
        # # perlbench_s is not working as it uses POSIX open function.
        # 'perlbench_s': {
        #     'name': '600.perlbench_s',
        #     'links': [],
        #     'skip_inst': 100000000,
        #     'max_insts': 100000000,
        #     'trace_func': '',
        # },

    }

    def __init__(self, trace_stdlib, force=False):
        self.spec = os.environ.get('SPEC')
        if self.spec is None:
            print(
                'Unable to find SPEC env var. Please source shrc before using this script')
            sys.exit()

        # Remember to setup gllvm.
        self.set_gllvm()

        self.trace_stdlib = trace_stdlib
        self.force = force

        Util.call_helper(['mkdir', '-p', 'result'])

        self.benchmarks = dict()
        for target in SPEC2017Benchmarks.BENCHMARK_PARAMS:
            name = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['name']
            max_insts = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['max_insts']
            skip_inst = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['skip_inst']
            trace_func = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['trace_func']
            links = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['links']

            # If we want to trace the std library, then we have to change the links.
            if self.trace_stdlib:
                links = [
                    '-static',
                    '-nostdlib',
                    C.MUSL_LIBC_CRT,
                    C.MUSL_LIBC_STATIC_LIB,
                    C.LLVM_COMPILER_RT_BUILTIN_LIB,
                    C.LLVM_UNWIND_STATIC_LIB,
                    # '-lgcc_s',
                    # '-lgcc',
                    # C++ run time library for tracer.
                    '-lstdc++',
                    # '-lc++',
                    # '-lc++abi',
                ]

            self.benchmarks[target] = SPEC2017Benchmark(
                self.force,
                name=name,
                target=target,
                max_insts=max_insts,
                links=links,
                trace_func=trace_func,
                skip_inst=skip_inst,
            )

    def set_gllvm(self):
        # Set up the environment for gllvm.
        os.putenv('LLVM_CC_NAME', 'ecc')
        os.putenv('LLVM_CXX_NAME', 'ecc++')
        os.putenv('LLVM_LINK_NAME', 'llvm-link')
        os.putenv('LLVM_AR_NAME', 'ecc-ar')


class ADFAAnalyzer:

    SYS_CPU_PREFIX = 'system.cpu.'

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
        l1_cache_misses = list()
        l2_cache_misses = list()
        for b in benchmarks:
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
            n_l1_cache_misses = baseline[ADFAAnalyzer.SYS_CPU_PREFIX +
                                         'dcache.demand_misses::cpu.data']
            n_l2_cache_misses = baseline['system.l2.overall_misses::cpu.data']

            branches.append(n_branch / n_insts * 1000.0)
            branch_misses.append(n_branch_misses / n_insts * 1000.0)
            l1_cache_misses.append(n_l1_cache_misses / n_insts * 1000.0)
            l2_cache_misses.append(n_l2_cache_misses / n_insts * 1000.0)

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
            '{l1cmpki:>7} '
            '{l2cmpki:>7} '
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
            l1cmpki='L1CMPKI',
            l2cmpki='L2CMPKI',
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
            '{l1cmpki:>7.1f} '
            '{l2cmpki:>7.1f} '
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
                l1cmpki=l1_cache_misses[i],
                l2cmpki=l2_cache_misses[i],
            ))


def main(folder):
    trace_stdlib = False
    force = False
    benchmarks = SPEC2017Benchmarks(trace_stdlib, force)
    names = list()
    b_list = list()
    for benchmark_name in benchmarks.benchmarks:
        benchmark = benchmarks.benchmarks[benchmark_name]
        benchmark.trace()
        benchmark.replay()
        b_list.append(benchmark)
    ADFAAnalyzer.analyze_adfa(b_list)


if __name__ == '__main__':
    import sys
    if len(sys.argv) == 1:
        # Use the current folder.
        main('.')
    else:
        main(sys.argv[1])
