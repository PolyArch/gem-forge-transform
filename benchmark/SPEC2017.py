import os
import sys
import subprocess
import multiprocessing

import Constants as C
import Util
import StreamStatistics
from Benchmark import Benchmark


"""
Top level functions to use JobScheduler.
"""


def trace(benchmark):
    benchmark.trace()


def collect_statistics(benchmark, trace_file, profile, debugs):
    os.chdir(benchmark.get_run_path())
    benchmark.benchmark.get_trace_statistics(trace_file, profile, debugs)
    os.chdir(benchmark.cwd)


def build_replay(benchmark, pass_name, trace_file, profile_file, output_tdg, debugs):
    os.chdir(benchmark.get_run_path())
    benchmark.benchmark.build_replay(
        pass_name=pass_name,
        trace_file=trace_file,
        profile_file=profile_file,
        tdg_detail='standalone',
        output_tdg=output_tdg,
        debugs=debugs,
    )
    os.chdir(benchmark.cwd)


def run_replay(benchmark, output_tdg, result, debugs):
    os.chdir(benchmark.get_run_path())
    gem5_outdir = benchmark.benchmark.gem5_replay(
        standalone=1,
        output_tdg=output_tdg,
        debugs=debugs,
    )
    Util.call_helper([
        'cp',
        os.path.join(gem5_outdir, 'region.stats.txt'),
        result,
    ])
    os.chdir(benchmark.cwd)


class SPEC2017Benchmark:

    def __init__(self, force_rebuild, **params):

        # Job ids.
        self.trace_job_id = None
        self.transform_job_ids = dict()

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

    def get_profile(self):
        return self.get_trace() + '.profile'

    def get_traces(self):
        traces = list()
        for i in xrange(self.n_traces):
            if self.target == 'gcc_s' and i == 7:
                # So far there is a but causing tdg #7 not transformable.
                continue
            traces.append('{target}.trace.{i}'.format(
                target=self.target, i=i))
        return traces

    def get_tdgs(self, transform):
        tdgs = list()
        for i in xrange(self.n_traces):
            if self.target == 'gcc_s' and i == 7:
                # So far there is a but causing tdg #7 not transformable.
                continue
            tdgs.append('{run}/{target}.{transform}.tdg.{i}'.format(
                run=self.get_run_path(),
                target=self.target,
                transform=transform,
                i=i))
        return tdgs

    def get_result(self, transform):
        results = list()
        for i in xrange(self.n_traces):
            if self.target == 'gcc_s' and i == 7:
                # So far there is a but causing tdg #7 not transformable.
                continue
            results.append(
                os.path.join(
                    self.cwd,
                    'result',
                    'spec.{name}.{transform}.txt.{i}'.format(
                        name=self.name, transform=transform, i=i
                    )))
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
        debugs = [
            'TracePass',
        ]
        self.benchmark.build_trace(
            debugs=debugs,
        )
        # set the maximum number of insts.
        if self.max_inst != -1:
            print(self.max_inst)
            print(self.start_inst)
            print(self.end_inst)
            print(self.skip_inst)
            os.putenv('LLVM_TDG_MAX_INST', str(int(self.max_inst)))
            os.putenv('LLVM_TDG_START_INST', str(int(self.start_inst)))
            os.putenv('LLVM_TDG_END_INST', str(int(self.end_inst)))
            os.putenv('LLVM_TDG_SKIP_INST', str(int(self.skip_inst)))
        else:
            os.unsetenv('LLVM_TDG_MAX_INST')
        self.benchmark.run_trace(self.get_trace())
        os.chdir(self.cwd)

    def schedule_trace(self, job_scheduler):
        # Add the trace job.
        # Notice that there is no dependence for trace job.
        deps = list()
        self.trace_job_id = (
            job_scheduler.add_job('{name}.trace'.format(
                name=self.get_name()), trace, (self,), deps)
        )

    def schedule_statistics(self, job_scheduler, debugs):
        traces = self.get_traces()
        profile = self.get_profile()
        deps = list()
        if self.trace_job_id is not None:
            deps.append(self.trace_job_id)
        job_scheduler.add_job(
            '{name}.statistics'.format(name=self.get_name()),
            collect_statistics,
            (self, traces[0], profile, debugs),
            deps
        )

    def schedule_transform(self, job_scheduler, transform, debugs):
        pass_name = 'replay'
        if transform == 'simd':
            pass_name = 'simd-pass'
        elif transform == 'adfa':
            pass_name = 'abs-data-flow-acc-pass'
        elif transform == 'replay':
            pass_name = 'replay'
        elif transform == 'stream':
            pass_name = 'stream-pass'
        else:
            raise ValueError('Unknown transform name!')
        profile_file = self.get_profile()
        traces = self.get_traces()
        tdgs = self.get_tdgs(transform)
        assert(len(traces) == len(tdgs))
        self.transform_job_ids[transform] = list()
        for i in xrange(0, len(traces)):
            # for i in [1]:
            deps = list()
            if self.trace_job_id is not None:
                deps.append(self.trace_job_id)

            # Schedule the build_replay job.
            self.transform_job_ids[transform].append(
                job_scheduler.add_job(
                    '{name}.transform.{transform}'.format(
                        name=self.get_name(), transform=transform),
                    build_replay,
                    (self, pass_name, traces[i],
                     profile_file, tdgs[i], debugs),
                    deps)
            )
        # assert(len(self.transform_job_ids[transform]) == len(traces))

    def schedule_simulation(self, job_scheduler, transform, debugs):
        tdgs = self.get_tdgs(transform)
        results = self.get_result(transform)
        assert(len(tdgs) == len(results))
        for i in xrange(0, len(tdgs)):
            deps = list()
            if transform in self.transform_job_ids:
                assert(len(self.transform_job_ids[transform]) == len(tdgs))
                deps.append(self.transform_job_ids[transform][i])

            job_scheduler.add_job(
                '{name}.simulate.{transform}'.format(
                    name=self.get_name(), transform=transform),
                run_replay,
                (self, tdgs[i], results[i], debugs),
                deps,
            )


class SPEC2017Benchmarks:

    BENCHMARK_PARAMS = {
        # 'lbm_s': {
        #     'name': '619.lbm_s',
        #     'links': ['-lm'],
        #     # First 100m insts every 1b insts, skipping the first 3.3b
        #     'start_inst': 33e8,
        #     'max_inst': 1e8,
        #     'skip_inst': 9e8,
        #     'end_inst': 34e8,
        #     'n_traces': 1,
        #     'trace_func': 'LBM_performStreamCollideTRT',
        # },
        # 'imagick_s': {
        #     'name': '638.imagick_s',
        #     'links': ['-lm'],
        #     'start_inst': 1e7,
        #     'max_inst': 1e7,
        #     'skip_inst': 9e8,
        #     'end_inst': 200e8,
        #     'n_traces': 22,
        #     'trace_func': 'MagickCommandGenesis',
        # },
        # 'nab_s': {
        #     'name': '644.nab_s',
        #     'links': ['-lm'],
        #     # md start from 100b
        #     'start_inst': 0,
        #     'max_inst': 1e8,
        #     'skip_inst': 9e8,
        #     'end_inst': 121e8,
        #     'n_traces': 1,
        #     'trace_func': 'md',
        # },
        # 'x264_s': {
        #     'name': '625.x264_s',
        #     'links': [],
        #     # x264_encoder_encode starts around 0.1e8.
        #     # x264_adaptive_quant_frame starts around 4e8
        #     # x264_lookahead_get_frames starts around 21e8
        #     'start_inst': 4e8,
        #     'max_inst': 1e8,
        #     'skip_inst': 20e8,
        #     'end_inst': 28e8,
        #     'n_traces': 2,
        #     'trace_func': 'x264_encoder_encode',
        # },
        # 'mcf_s': {
        #     'name': '605.mcf_s',
        #     'links': [],
        #     # global_opt starts around 0.3e8
        #     'start_inst': 1e8,
        #     'max_inst': 1e8,
        #     'skip_inst': 9e8,
        #     'end_inst': 21e8,
        #     'n_traces': 2,
        #     'trace_func': 'global_opt',
        # },
        # 'xz_s': {
        #     'name': '657.xz_s',
        #     'links': [],
        #     # I failed to find the pattern.
        #     # To trace this, instrument the tracer twice, one with
        #     # compressStream traced, the other with uncompressStream traced
        #     'start_inst': 0,
        #     'max_inst': 1e8,
        #     'skip_inst': 9e8,
        #     'end_inst': 1021e8,
        #     'n_traces': 1,
        #     'trace_func': 'uncompressStream',
        # },
        # 'gcc_s': {
        #     'name': '602.gcc_s',
        #     'links': [],
        #     'start_inst': 1e8,
        #     'max_inst': 1e7,
        #     'skip_inst': 50e8,
        #     'end_inst': 1000e8,
        #     'n_traces': 20,
        #     'trace_func': 'compile_file',
        # },
        # # C++ Benchmark
        'deepsjeng_s': {
            'name': '631.deepsjeng_s',
            'links': [],
            'start_inst': 0,
            'max_inst': 1e8,
            'skip_inst': 0,
            'end_inst': 0,
            'n_traces': 1,
            'trace_func': 'think(gamestate_t*, state_t*)',
        },
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


def main(folder, build, trace):
    trace_stdlib = False
    job_scheduler = Util.JobScheduler(8, 1)
    benchmarks = SPEC2017Benchmarks(trace_stdlib, build)
    names = list()
    b_list = list()
    for benchmark_name in benchmarks.benchmarks:
        benchmark = benchmarks.benchmarks[benchmark_name]
        if trace:
            benchmark.schedule_trace(job_scheduler)
        # benchmark.schedule_statistics(job_scheduler, [])
        debugs = [
            'ReplayPass',
            'StreamPass',
            # 'DataGraph',
        ]
        # benchmark.schedule_transform(job_scheduler, 'stream', debugs)
        # debugs = []
        # benchmark.schedule_simulation(job_scheduler, 'stream', debugs)
        # debugs = [
        #     'ReplayPass',
        #     'PostDominanceFrontier',
        #     # 'TDGSerializer',
        #     # 'AbstractDataFlowAcceleratorPass',
        #     # 'LoopUtils',
        # ]
        # benchmark.schedule_transform(job_scheduler, 'replay', debugs)
        # debugs = []
        # benchmark.schedule_simulation(job_scheduler, 'replay', debugs)
        # debugs = [
        #     'ReplayPass',
        # ]
        # benchmark.schedule_transform(job_scheduler, 'adfa', debugs)
        # debugs = []
        # benchmark.schedule_simulation(job_scheduler, 'adfa', debugs)
        # debugs = [
        #     'ReplayPass',
        # ]
        # benchmark.schedule_transform(job_scheduler, 'simd', debugs)
        # debugs = []
        # benchmark.schedule_simulation(job_scheduler, 'simd', debugs)

        b_list.append(benchmark)

    # Start the job.
    job_scheduler.run()

    # for benchmark in b_list:
    #     tdgs = benchmark.get_tdgs('stream')
    #     tdg_stats = [tdg + '.stats.txt' for tdg in tdgs]
    # stream_stats = StreamStatistics.StreamStatistics(tdg_stats)
    # print('-------------------------- ' + benchmark.get_name())
    # stream_stats.print_stats()
    # stream_stats.print_access()

    # Util.ADFAAnalyzer.SYS_CPU_PREFIX = 'system.cpu.'
    # Util.ADFAAnalyzer.analyze_adfa(b_list)


if __name__ == '__main__':
    import optparse
    parser = optparse.OptionParser()
    parser.add_option('-b', '--build', action='store_true',
                      dest='build', default=False)
    parser.add_option('-t', '--trace', action='store_true',
                      dest='trace', default=False)
    parser.add_option('-d', '--directory', action='store',
                      type='string', dest='directory')
    (options, args) = parser.parse_args()
    # Use the current folder.
    main(options.directory, options.build, options.trace)
