import os
import sys
import subprocess
import multiprocessing

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
        self.trace_func = params['trace_func']

        self.start_inst = params['start_inst']
        self.max_inst = params['max_inst']
        self.skip_inst = params['skip_inst']
        self.end_inst = params['end_inst']
        self.n_traces = params['n_traces']

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

    def get_traces(self):
        traces = list()
        for i in xrange(self.n_traces):
            traces.append('{target}.trace.{i}'.format(
                target=self.target, i=i))
        return traces

    def get_tdgs(self, transform):
        tdgs = list()
        for i in xrange(self.n_traces):
            tdgs.append('{target}.{transform}.tdg.{i}'.format(
                target=self.target, transform=transform, i=i))
        return tdgs

    def get_results(self, transform):
        results = list()
        for i in xrange(self.n_traces):
            results.append('spec.{name}.{transform}.txt.{i}'.format(
                name=self.name, transform=transform, i=i
            ))
        return results

    def build_raw_bc(self):
        # Fake the runcpu.
        fake_cmd = [
            'runcpu',
            '--fake',
            '--config=ecc-llvm-linux-x86_64',
            '--tune=base',
            self.name
        ]
        Util.call_helper(fake_cmd)
        # Actually build it.
        os.chdir(self.get_build_path())
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
        if self.target == 'gcc_s':
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
        if self.max_inst != -1:
            print(self.max_inst)
            print(self.start_inst)
            print(self.end_inst)
            print(self.skip_inst)
            os.putenv('LLVM_TDG_MAX_INST', str(self.max_inst))
            os.putenv('LLVM_TDG_START_INST', str(self.start_inst))
            os.putenv('LLVM_TDG_END_INST', str(self.end_inst))
            os.putenv('LLVM_TDG_SKIP_INST', str(self.skip_inst))
        else:
            os.unsetenv('LLVM_TDG_MAX_INST')
        self.benchmark.run_trace(self.get_trace())
        os.chdir(self.cwd)

    def build_replay(self):
        pass_name = 'replay'
        debugs = [
            'ReplayPass',
            # 'DataGraph',
            'DynamicInstruction',
        ]
        traces = self.get_traces()
        tdgs = self.get_tdgs('replay')
        assert(len(traces) == len(tdgs))
        for i in xrange(len(traces)):
            self.benchmark.build_replay(
                pass_name=pass_name,
                trace_file=traces[i],
                tdg_detail='standalone',
                output_tdg=tdgs[i],
                debugs=debugs,
            )

    def build_replay_abs_data_flow(self):
        pass_name = 'abs-data-flow-acc-pass'
        debugs = [
            'ReplayPass',
            # 'TDGSerializer',
            # 'AbstractDataFlowAcceleratorPass',
            # 'LoopUtils',
        ]
        traces = self.get_traces()
        tdgs = self.get_tdgs('adfa')
        assert(len(traces) == len(tdgs))
        for i in xrange(len(traces)):
            self.benchmark.build_replay(
                pass_name=pass_name,
                trace_file=traces[i],
                tdg_detail='standalone',
                output_tdg=tdgs[i],
                debugs=debugs,
            )

    def build_replay_simd(self):
        pass_name = 'simd-pass'
        debugs = [
            'ReplayPass',
            # 'SIMDPass',
        ]
        traces = self.get_traces()
        tdgs = self.get_tdgs('simd')
        assert(len(traces) == len(tdgs))
        for i in xrange(len(traces)):
            self.benchmark.build_replay(
                pass_name=pass_name,
                trace_file=traces[i],
                tdg_detail='standalone',
                output_tdg=tdgs[i],
                debugs=debugs,
            )

    def run_replay(self, transform, debugs):
        tdgs = self.get_tdgs(transform)
        results = self.get_results(transform)
        assert(len(tdgs) == len(results))
        for i in xrange(len(tdgs)):
            gem5_dir = self.benchmark.gem5_replay(
                standalone=1,
                output_tdg=tdgs[i],
                debugs=debugs,
            )
            Util.call_helper([
                'cp',
                os.path.join(gem5_outdir, 'region.stats.txt'),
                results[i],
            ])

    def replay(self):
        os.chdir(self.get_run_path())
        # Basic replay.
        self.build_replay()
        debugs = [
            # 'LLVMTraceCPU'
        ]
        # self.run_replay('replay', debugs)
        # Abstract data flow replay.
        self.build_replay_abs_data_flow()
        # debugs = [
        #     # 'LLVMTraceCPU',
        #     # 'AbstractDataFlowAccelerator'
        # ]
        # self.run_replay('adfa', debugs)
        # SIMD replay.
        self.build_replay_simd()
        # debugs = [
        #     # 'LLVMTraceCPU',
        #     # 'AbstractDataFlowAccelerator'
        # ]
        # self.run_replay('simd', debugs)
        os.chdir(self.cwd)


class SPEC2017Benchmarks:

    BENCHMARK_PARAMS = {
        # 'lbm_s': {
        #     'name': '619.lbm_s',
        #     'links': ['-lm'],
        #     # First 100m insts every 1b insts, skipping the first 3.3b
        #     'start_inst': 33e8,
        #     'max_inst': 1e8,
        #     'skip_inst': 9e8,
        #     'end_inst': 54e8,
        #     'n_traces': 2,
        #     'trace_func': 'LBM_performStreamCollideTRT',
        # },
        'imagick_s': {
            'name': '638.imagick_s',
            'links': ['-lm'],
            'start_inst': 1e7,
            'max_inst': 1e8,
            'skip_inst': 9e8,
            'end_inst': 15e8,
            'n_traces': 2,
            'trace_func': 'MagickCommandGenesis',
        },
        # 'nab_s': {
        #     'name': '644.nab_s',
        #     'links': ['-lm'],
        #     # First 100m insts every 1b insts, skipping the first 100m
        #     'start_inst': 1e8,
        #     'max_inst': 1e8,
        #     'skip_inst': 9e8,
        #     'end_inst': 21e8,
        #     'trace_func': 'md',
        # },
        # 'x264_s': {
        #     'name': '625.x264_s',
        #     'links': [],
        #     # First 100m insts every 1b insts, skipping the first 100m
        #     'start_inst': 1e8,
        #     'max_inst': 1e8,
        #     'skip_inst': 9e8,
        #     'end_inst': 21e8,
        #     'trace_func': 'x264_encoder_encode',
        # },
        # 'mcf_s': {
        #     'name': '605.mcf_s',
        #     'links': [],
        #     # First 100m insts every 1b insts, skipping the first 100m
        #     'start_inst': 1e8,
        #     'max_inst': 1e8,
        #     'skip_inst': 9e8,
        #     'end_inst': 21e8,
        #     'trace_func': 'global_opt',
        # },
        # 'xz_s': {
        #     'name': '657.xz_s',
        #     'links': [],
        #     # First 100m insts every 1b insts, skipping the first 100m
        #     'start_inst': 1e8,
        #     'max_inst': 1e8,
        #     'skip_inst': 9e8,
        #     'end_inst': 21e8,
        #     'trace_func': 'spec_compress,spec_uncompress',
        # },
        # 'gcc_s': {
        #     'name': '602.gcc_s',
        #     'links': [],
        #     'skip_inst': 100000000,
        #     'max_inst': 100000000,
        #     'trace_func': '',
        # },
        # # C++ Benchmark: Notice that SPEC is compiled without
        # # debug information, and we have to use mangled name
        # # for trace_func.
        # 'deepsjeng_s': {
        #     'name': '631.deepsjeng_s',
        #     'links': [],
        #     'skip_inst': 100000000,
        #     'max_inst': 100000000,
        #     'trace_func': '',
        # },
        # 'leela_s': {
        #     'name': '641.leela_s',
        #     'links': [],
        #     'skip_inst': 0,
        #     'max_inst': 100000000,
        #     'trace_func': '_ZN9UCTSearch5thinkEii', # Search::think
        # },
        # # perlbench_s is not working as it uses POSIX open function.
        # 'perlbench_s': {
        #     'name': '600.perlbench_s',
        #     'links': [],
        #     'skip_inst': 100000000,
        #     'max_inst': 100000000,
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

            start_inst = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['start_inst']
            max_inst = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['max_inst']
            skip_inst = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['skip_inst']
            end_inst = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['end_inst']
            n_traces = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['n_traces']

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
                links=links,
                trace_func=trace_func,
                start_inst=start_inst,
                max_inst=max_inst,
                skip_inst=skip_inst,
                end_inst=end_inst,
                n_traces=n_traces,
            )

    def set_gllvm(self):
        # Set up the environment for gllvm.
        os.putenv('LLVM_CC_NAME', 'ecc')
        os.putenv('LLVM_CXX_NAME', 'ecc++')
        os.putenv('LLVM_LINK_NAME', 'llvm-link')
        os.putenv('LLVM_AR_NAME', 'ecc-ar')


def run_benchmark(benchmark):
    print('start run benchmark ' + benchmark.get_name())
    # benchmark.trace()
    benchmark.replay()


def main(folder):
    trace_stdlib = False
    force = False
    benchmarks = SPEC2017Benchmarks(trace_stdlib, force)
    names = list()
    b_list = list()
    processes = list()
    for benchmark_name in benchmarks.benchmarks:
        benchmark = benchmarks.benchmarks[benchmark_name]
        processes.append(multiprocessing.Process(
            target=run_benchmark, args=(benchmark,)))
        b_list.append(benchmark)

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

    # Util.ADFAAnalyzer.SYS_CPU_PREFIX = 'system.cpu.'
    # Util.ADFAAnalyzer.analyze_adfa(b_list)


if __name__ == '__main__':
    import sys
    if len(sys.argv) == 1:
        # Use the current folder.
        main('.')
    else:
        main(sys.argv[1])
