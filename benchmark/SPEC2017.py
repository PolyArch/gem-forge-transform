import os
import sys
import subprocess
import multiprocessing

import Constants as C
import Util
import StreamStatistics
from Benchmark import Benchmark


class SPEC2017Benchmark(Benchmark):

    def __init__(self, **params):

        # Job ids.
        self.trace_job_id = None
        self.transform_job_ids = dict()

        self.spec = os.environ.get('SPEC')
        if self.spec is None:
            print(
                'Unable to find SPEC env var. Please source shrc before using this script')
            sys.exit()

        self.work_load = params['name']
        self.target = params['target']
        self.trace_func = params['trace_func']

        self.start_inst = params['start_inst']
        self.max_inst = params['max_inst']
        self.skip_inst = params['skip_inst']
        self.end_inst = params['end_inst']
        self.n_traces = params['n_traces']

        self.trace_stdlib = False
        self.cwd = os.getcwd()

        # Check if build dir and run dir exists.
        if not (os.path.isdir(self.get_build_path()) and os.path.isdir(self.get_run_path())):
            self.build_raw_bc()

        # Finally use specinvoke to get the arguments.
        os.chdir(self.get_run_path())
        args = self.get_args(subprocess.check_output([
            'specinvoke',
            '-n'
        ]))
        print('{name} has arguments {args}'.format(name=self.get_name(), args=args))
        os.chdir(self.cwd)

        # Initialize the benchmark.
        super(SPEC2017Benchmark, self).__init__(
            name=self.get_name(),
            raw_bc=self.get_raw_bc(),
            links=params['links'],
            args=args,
            trace_func=self.trace_func,
            lang=params['lang'],
        )

    def get_name(self):
        return 'spec.' + self.work_load

    def get_build_path(self):
        return os.path.join(
            self.spec,
            'benchspec',
            'CPU',
            self.work_load,
            'build',
            'build_base_LLVM_TDG-m64.0000'
        )

    def get_run_path(self):
        if self.work_load.endswith('_s'):
            run_dirname = 'run_base_refspeed_LLVM_TDG-m64.0000'
        else:
            run_dirname = 'run_base_refrate_LLVM_TDG-m64.0000'
        return os.path.join(
            self.spec,
            'benchspec',
            'CPU',
            self.work_load,
            'run',
            run_dirname
        )

    def get_raw_bc(self):
        return self.target + '.bc'

    def get_n_traces(self):
        return self.n_traces

    def build_raw_bc(self):
        # Clear the existing build.
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
        # Fake the runcpu.
        fake_cmd = [
            'runcpu',
            '--fake',
            '--config=ecc-llvm-linux-x86_64',
            '--tune=base',
            self.work_load
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
                C.LLVM_LINK,
                raw_bc,
                C.MUSL_LIBC_LLVM_BC,
                '-o',
                raw_bc
            ]
            print('# Link with stdlib...')
            Util.call_helper(link_cmd)

        # Name everything in the bitcode.
        print('# Naming everything in the llvm bitcode...')
        Util.call_helper([C.OPT, '-instnamer', raw_bc, '-o', raw_bc])
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
            if self.work_load.endswith('namd_r'):
                # Sepcial case for namd_r, which echos to a script and use numactl to
                # control NUMA access.
                # I don't think we should consider this for our benchmark,
                # so I manually extracted the arguments.
                return [
                    '--input',
                    'apoa1.input',
                    '--output',
                    'apoa1.ref.output',
                    '--iterations',
                    '65',
                ]
            elif self.work_load.endswith('povray_r'):
                return [
                    'SPEC-benchmark-ref.ini',
                ]
            elif self.work_load.endswith('parest_r'):
                return [
                    'ref.prm',
                ]
            elif self.work_load.endswith('blender_r'):
                return [
                    'sh3_no_char.blend',
                    '--render-output',
                    'sh3_no_char_',
                    '--threads', '1', '-b', '-F',
                    'RAWTGA', '-s', '849', '-e', '849', '-a',
                ]
            # Ignore the first one and redirect one.
            # Also ignore other invoke for now.
            return fields[1:-5]

    def trace(self):
        os.chdir(self.get_run_path())
        debugs = [
            # 'TracePass',
        ]
        self.build_trace(
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
        self.run_trace(self.get_name())
        os.chdir(self.cwd)

    def transform(self, pass_name, trace_file, profile_file, output_tdg, debugs):
        os.chdir(self.get_run_path())
        self.build_replay(
            pass_name=pass_name,
            trace_file=trace_file,
            profile_file=profile_file,
            tdg_detail='standalone',
            output_tdg=output_tdg,
            debugs=debugs,
        )
        os.chdir(self.cwd)

    def simulate(self, output_tdg, result, debugs):
        os.chdir(self.get_run_path())
        gem5_outdir = self.gem5_replay(
            standalone=1,
            output_tdg=output_tdg,
            debugs=debugs,
        )
        Util.call_helper([
            'cp',
            os.path.join(gem5_outdir, 'region.stats.txt'),
            result,
        ])
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
        #     'end_inst': 34e8,
        #     'n_traces': 1,
        #     'trace_func': 'LBM_performStreamCollideTRT',
        #     'lang': 'C',
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
        #     'lang': 'C',
        # },
        'nab_s': {
            'name': '644.nab_s',
            'links': ['-lm'],
            # md start from 100b
            'start_inst': 0,
            'max_inst': 1e8,
            'skip_inst': 9e8,
            'end_inst': 121e8,
            'n_traces': 1,
            'trace_func': 'md',
            'lang': 'C',
        },
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
        #     'lang': 'C',
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
        #     'lang': 'C',
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
        #     'lang': 'C',
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
        #     'lang': 'C',
        # },
        # # Does not work with ellcc as it uses posix io function.
        # 'perlbench_s': {
        #     'name': '600.perlbench_s',
        #     'links': [],
        #     'start_inst': 1e8,
        #     'max_inst': 1e7,
        #     'skip_inst': 10e8,
        #     'end_inst': 100e8,
        #     'n_traces': 10,
        #     'trace_func': '',
        #     'lang': 'C',
        # },
        # # C++ Benchmark
        # 'deepsjeng_s': {
        #     'name': '631.deepsjeng_s',
        #     'links': [],
        #     'start_inst': 0,
        #     'max_inst': 1e8,
        #     'skip_inst': 0,
        #     'end_inst': 0,
        #     'n_traces': 1,
        #     'trace_func': 'think(gamestate_t*, state_t*)',
        #     'lang': 'CPP',
        # },
        # 'leela_s': {
        #     'name': '641.leela_s',
        #     'links': [],
        #     'start_inst': 1e8,
        #     'max_inst': 1e7,
        #     'skip_inst': 10e8,
        #     'end_inst': 100e8,
        #     'n_traces': 10,
        #     'trace_func': '',
        #     'lang': 'CPP',
        # },
        # 'namd_r': {
        #     'name': '508.namd_r',
        #     'links': [],
        #     'start_inst': 1e8,
        #     'max_inst': 1e7,
        #     'skip_inst': 10e8,
        #     'end_inst': 100e8,
        #     'n_traces': 10,
        #     'trace_func': '',
        #     'lang': 'CPP',
        # },
        # # Will throw exception.
        # # Does not work with ellcc as RE.
        # 'povray_r': {
        #     'name': '511.povray_r',
        #     'links': [],
        #     'start_inst': 1e8,
        #     'max_inst': 1e7,
        #     'skip_inst': 10e8,
        #     'end_inst': 100e8,
        #     'n_traces': 10,
        #     'trace_func': '',
        #     'lang': 'CPP',
        # },
        # # Throws exception.
        # # Does not work with ellcc as RE.
        # # Need to fix a comparison between integer and pointer error.
        # 'parest_r': {
        #     'name': '510.parest_r',
        #     'links': [],
        #     'start_inst': 1e8,
        #     'max_inst': 1e7,
        #     'skip_inst': 10e8,
        #     'end_inst': 100e8,
        #     'n_traces': 10,
        #     'trace_func': '',
        #     'lang': 'CPP',
        # },

        # # Does not work with ellcc as it uses linux header.
        # # Does not throw.
        # 'xalancbmk_s': {
        #     'name': '623.xalancbmk_s',
        #     'links': [],
        #     'start_inst': 1e8,
        #     'max_inst': 1e7,
        #     'skip_inst': 10e8,
        #     'end_inst': 100e8,
        #     'n_traces': 10,
        #     'trace_func': '',
        #     'lang': 'CPP',
        # },

        # # Portablity issue with using std::isfinite but include <math.h>, not <cmath>
        # # Does not throw.
        # # Haven't tested with ellcc.
        # 'blender_r': {
        #     'name': '526.blender_r',
        #     'links': [],
        #     'start_inst': 1e8,
        #     'max_inst': 1e7,
        #     'skip_inst': 10e8,
        #     'end_inst': 100e8,
        #     'n_traces': 10,
        #     'trace_func': '',
        #     'lang': 'CPP',
        # },


        # # Not working so far due to setjmp/longjmp.
        # 'omnetpp_s': {
        #     'name': '620.omnetpp_s',
        #     'links': [],
        #     'start_inst': 1e8,
        #     'max_inst': 1e7,
        #     'skip_inst': 10e8,
        #     'end_inst': 100e8,
        #     'n_traces': 10,
        #     'trace_func': '',
        #     'lang': 'CPP',
        # },

    }

    def __init__(self):
        self.spec = os.environ.get('SPEC')
        if self.spec is None:
            print(
                'Unable to find SPEC env var. Please source shrc before using this script')
            sys.exit()

        # Remember to setup gllvm.
        self.set_gllvm()

        self.benchmarks = list()
        for target in SPEC2017Benchmarks.BENCHMARK_PARAMS:
            name = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['name']

            start_inst = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['start_inst']
            max_inst = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['max_inst']
            skip_inst = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['skip_inst']
            end_inst = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['end_inst']
            n_traces = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['n_traces']
            lang = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['lang']

            trace_func = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['trace_func']
            links = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['links']

            # If we want to trace the std library, then we have to change the links.
            # if self.trace_stdlib:
            #     links = [
            #         '-static',
            #         '-nostdlib',
            #         C.MUSL_LIBC_CRT,
            #         C.MUSL_LIBC_STATIC_LIB,
            #         C.LLVM_COMPILER_RT_BUILTIN_LIB,
            #         C.LLVM_UNWIND_STATIC_LIB,
            #         # '-lgcc_s',
            #         # '-lgcc',
            #         # C++ run time library for tracer.
            #         '-lstdc++',
            #         # '-lc++',
            #         # '-lc++abi',
            #     ]

            self.benchmarks.append(
                SPEC2017Benchmark(
                    name=name,
                    target=target,
                    links=links,
                    trace_func=trace_func,
                    start_inst=start_inst,
                    max_inst=max_inst,
                    skip_inst=skip_inst,
                    end_inst=end_inst,
                    n_traces=n_traces,
                    lang=lang,
                )
            )

    def get_benchmarks(self):
        return self.benchmarks

    def set_gllvm(self):
        # Set up the environment for gllvm.
        if C.USE_ELLCC:
            os.putenv('LLVM_COMPILER_PATH', C.ELLCC_BIN_PATH)
            os.putenv('LLVM_CC_NAME', 'ecc')
            os.putenv('LLVM_CXX_NAME', 'ecc++')
            os.putenv('LLVM_LINK_NAME', 'llvm-link')
            os.putenv('LLVM_AR_NAME', 'ecc-ar')
        else:
            os.putenv('LLVM_COMPILER_PATH', C.LLVM_BIN_PATH)
            os.putenv('LLVM_CC_NAME', 'clang')
            os.putenv('LLVM_CXX_NAME', 'clang++')
            os.putenv('LLVM_LINK_NAME', 'llvm-link')
            os.putenv('LLVM_AR_NAME', 'llvm-ar')
