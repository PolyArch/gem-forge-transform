import os
import sys
import subprocess
import multiprocessing

from Utils import TraceFlagEnum

import Constants as C
import Util
from Benchmark import Benchmark


class SPEC2017Benchmark(Benchmark):

    def __init__(self, benchmark_args, **params):

        # Job ids.
        self.trace_job_id = None
        self.transform_job_ids = dict()

        self.spec = os.environ.get('SPEC2017_SUITE_PATH')
        if self.spec is None:
            print(
                'Unable to find SPEC2017_SUITE_PATH env var. Please source shrc before using this script')
            sys.exit()

        self.work_load = params['name']
        self.target = params['target']
        self.trace_func = params['trace_func']

        self.trace_stdlib = False
        self.cwd = os.getcwd()

        self.build_system = 'gclang'
        self.build_system = 'clang'

        # Create the run_path.
        self.run_path = os.path.join(
            C.LLVM_TDG_RESULT_DIR, 'spec2017', self.work_load)
        Util.mkdir_chain(self.run_path)

        # Check if build dir and exe dir exists.
        if not (os.path.isdir(self.get_build_path()) and os.path.isdir(self.get_exe_path())):
            self.runcpu_fake()

        # Finally use specinvoke to get the arguments.
        os.chdir(self.get_exe_path())
        self.args = self.find_args(subprocess.check_output([
            'specinvoke',
            '-n'
        ]))
        print('{name} has arguments {args}'.format(
            name=self.get_name(), args=self.args))
        os.chdir(self.cwd)

        # Initialize the benchmark.
        self.links = params['links']
        self.lang = params['lang']
        super(SPEC2017Benchmark, self).__init__(
            benchmark_args,
        )

    def get_name(self):
        return 'spec.' + self.work_load

    def get_links(self):
        return self.links

    def get_args(self):
        return self.args

    def get_trace_func(self):
        return self.trace_func

    def get_lang(self):
        return self.lang

    def get_perf_frequency(self):
        # Spec are so long, perf at a low frequency.
        return 1

    def get_build_label(self):
        return 'LLVM_TDG_{build_system}'.format(
            build_system=self.build_system.upper()
        )

    def get_config_file(self):
        return '{build_system}-llvm-linux-x86_64-{expr}.cfg'.format(
            build_system=self.build_system,
            # For now we use a special experiment setting for gem5.
            expr=C.EXPERIMENTS + '.gem5',
        )

    def get_benchmark_path(self):
        return os.path.join(
            self.spec,
            'benchspec',
            'CPU',
            self.work_load,
        )

    def get_build_path(self):
        return os.path.join(
            self.get_benchmark_path(),
            'build',
            'build_base_{label}-m64.0000'.format(
                label=self.get_build_label())
        )

    def get_exe_path(self):
        if self.work_load.endswith('_s'):
            run_dirname = 'run_base_refspeed_{label}-m64.0000'.format(
                label=self.get_build_label()
            )
        else:
            run_dirname = 'run_base_refrate_{label}-m64.0000'.format(
                label=self.get_build_label()
            )
        return os.path.join(
            self.get_benchmark_path(),
            'run',
            run_dirname
        )

    def get_run_path(self):
        return self.run_path

    def get_hard_exit_in_billion(self):
        if self.get_name() == 'spec.620.omnetpp_s':
            return 50
        return 10

    def runcpu_fake(self):
        # Clear the existing build.
        clear_cmd = [
            'rm',
            '-rf',
            os.path.join(
                self.get_benchmark_path(),
                'build'
            )
        ]
        Util.call_helper(clear_cmd)
        clear_cmd = [
            'rm',
            '-rf',
            os.path.join(
                self.get_benchmark_path(),
                'run'
            )
        ]
        Util.call_helper(clear_cmd)
        # Fake the runcpu.
        fake_cmd = [
            'runcpu',
            '--fake',
            '--config={config}'.format(
                config=self.get_config_file()
            ),
            '--tune=base',
            self.work_load
        ]
        Util.call_helper(fake_cmd)

        # Special case for x264_s: we have to create a symbolic link to the input.
        if self.work_load == '625.x264_s':
            input_file = os.path.join(self.get_exe_path(), 'BuckBunny.yuv')
            original_input_file = os.path.join(
                self.get_exe_path(), '../../BuckBunny.yuv')
            Util.create_symbolic_link(original_input_file, input_file)

    def build_raw_bc(self):
        self.runcpu_fake()
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
        binary = self.target
        if self.target == 'gcc_s':
            # Special rule for gcc_s, whose binary is sgcc
            binary = 'sgcc'
        elif self.target == 'xalancbmk_r':
            binary = 'cpuxalan_r'

        if self.build_system == 'gclang':
            # Extract the bitcode from the binary.
            extract_cmd = [
                'get-bc',
                '-o',
                raw_bc,
                binary
            ]
            print('# Extracting bitcode from binary...')
            Util.call_helper(extract_cmd)
        elif self.build_system == 'clang':
            # With LTO and save-temps plugin options we should have
            # {binary}.0.5.precodegen.bc
            mv_cmd = [
                'mv',
                '{binary}.0.5.precodegen.bc'.format(binary=binary),
                raw_bc
            ]
            print('# Renaming precodegen bitcode...')
            Util.call_helper(mv_cmd)
        else:
            # Unknown build system.
            assert(False)
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
        # Copy it to exe directory.
        Util.call_helper([
            'cp',
            raw_bc,
            self.get_exe_path()
        ])
        # Also copy it to run directory.
        Util.call_helper([
            'cp',
            raw_bc,
            self.get_run_path()
        ])
        os.chdir(self.cwd)

    def find_args(self, specinvoke):
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
            elif self.work_load.endswith('xalancbmk_r'):
                return ['-v', 't5.xml', 'xalanc.xsl']
            elif self.work_load.endswith('xz_r'):
                return [
                    'cld.tar.xz',
                    '160',
                    '19cf30ae51eddcbefda78dd06014b4b96281456e078ca7c13e1c0c9e6aaea8dff3efb4ad6b0456697718cede6bd5454852652806a657bb56e07d61128434b474',
                    '59796407',
                    '61004416',
                    '6',
                ]
            # Ignore the first one and redirect one.
            # Also ignore other invoke for now.
            return fields[1:-5]

    def trace(self):
        os.chdir(self.get_exe_path())
        self.build_trace(
            link_stdlib=False,
        )
        # Set the tracer mode.

        os.putenv('LLVM_TDG_TRACE_MODE', str(
            TraceFlagEnum.GemForgeTraceMode.TraceSpecifiedInterval.value
        ))
        os.putenv('LLVM_TDG_INTERVALS_FILE', self.get_simpoint_abs())
        os.unsetenv('LLVM_TDG_TRACE_ROI')
        self.run_trace()
        os.chdir(self.cwd)

    def get_region_simpoint_candidate_edge_min_insts(self):
        if self.get_name() == 'spec.605.mcf_s':
            # mcf_s is quite fragmented, we use 100k as threshold.
            return 100 * 1000
        if self.get_name() == 'spec.638.imagick_s':
            # imagick_s is a little fragmented, we use 1 million threshold.
            return 1000 * 1000
        if self.get_name() == 'spec.508.namd_r':
            # namd_r is a little fragmented in calc_pair_energy, we use 1 million threshold.
            return 1000 * 1000
        if self.get_name() == 'spec.557.xz_r':
            return 1000 * 1000
        if self.get_name() == 'spec.523.xalancbmk_r':
            return 50 * 1000
        # Others we use 10 million as default.
        return 10000000

    def get_additional_gem5_simulate_command(self):
        args = []
        """
        Some benchmarks takes too long to finish, so we use work item
        to ensure that we simualte for the same amount of work.
        """
        work_items = -1
        if self.get_name() == 'spec.508.namd_r':
            # Just run one iteration.
            work_items = 2
        elif self.get_name() == 'spec.605.mcf_s':
            # Run 1 primal_bea_mpp
            work_items = 1024
        elif self.get_name() == 'spec.638.imagick_s':
            # Run 1024 iteration of HorizontalFilter
            work_items = 1024
        if work_items != -1:
            args += [
                '--work-end-exit-count={v}'.format(v=work_items)
            ]
        return args

    def get_gem5_mem_size(self):
        large_mem_benchmarks = [
            'spec.657.xz_s'
        ]
        for p in large_mem_benchmarks:
            if self.get_name().startswith(p):
                return '16GB'
        return '2GB'


class SPEC2017Benchmarks:

    BENCHMARK_PARAMS = {
        'lbm_s': {
            'name': '619.lbm_s',
            'links': ['-lm'],
            # First 100m insts every 1b insts, skipping the first 3.3b
            'trace_func': 'LBM_performStreamCollideTRT',
            'lang': 'C',
        },
        'imagick_s': {
            'name': '638.imagick_s',
            'links': ['-lm'],
            # 'trace_func': 'MagickCommandGenesis',
            'trace_func': Benchmark.ROI_FUNC_SEPARATOR.join([
                # 'ReadTGAImage',
                'HorizontalFilter', 
                'VerticalFilter',
                'WritePixelCachePixels',
            ]),
            'lang': 'C',
        },
        'nab_s': {
            'name': '644.nab_s',
            'links': ['-lm'],
            # md start from 100b
            'trace_func': 'md',
            'lang': 'C',
        },
        'x264_s': {
            'name': '625.x264_s',
            'links': ['-lm'],
            # x264_encoder_encode starts around 0.1e8.
            # x264_adaptive_quant_frame starts around 4e8
            # x264_lookahead_get_frames starts around 21e8
            'trace_func': 'x264_encoder_encode',
            'lang': 'C',
        },
        'mcf_s': {
            'name': '605.mcf_s',
            'links': [],
            # global_opt starts around 0.3e8
            # 'trace_func': 'global_opt',
            'trace_func': 'primal_bea_mpp',
            'lang': 'C',
        },
        'xz_r': {
            'name': '557.xz_r',
            'links': [],
            'trace_func': Benchmark.ROI_FUNC_SEPARATOR.join([
                'lzma_decode',
            ]),
            'lang': 'C',
        },
        'xz_s': {
            'name': '657.xz_s',
            'links': [],
            # I failed to find the pattern.
            # To trace this, instrument the tracer twice, one with
            # compressStream traced, the other with uncompressStream traced
            'trace_func': Benchmark.ROI_FUNC_SEPARATOR.join(['uncompressStream', 'compressStream']),
            'lang': 'C',
        },
        'gcc_s': {
            'name': '602.gcc_s',
            'links': [],
            # 'trace_func': 'compile_file',
            'trace_func': '',
            'lang': 'C',
        },
        'perlbench_s': {
            'name': '600.perlbench_s',
            'links': [],
            'trace_func': '',
            'lang': 'C',
        },
        # C++ Benchmark
        'deepsjeng_s': {
            'name': '631.deepsjeng_s',
            'links': [],
            # 'trace_func': 'think(gamestate_t*, state_t*)',
            'trace_func': '',
            'lang': 'CPP',
        },
        'leela_s': {
            'name': '641.leela_s',
            'links': [],
            'trace_func': '',
            'lang': 'CPP',
        },
        'namd_r': {
            'name': '508.namd_r',
            'links': [],
            'trace_func': 'ComputeNonbondedUtil::calc_self_energy',
            'trace_func': Benchmark.ROI_FUNC_SEPARATOR.join([
                'ComputeNonbondedUtil::calc_self_energy',
                'Patch::zeroforces',
                'Patch::setforces',
            ]),
            'lang': 'CPP',
        },
        # Will throw exception.
        # Does not work with ellcc as RE.
        'povray_r': {
            'name': '511.povray_r',
            'links': [],
            'trace_func': '',
            'lang': 'CPP',
        },
        # Throws exception.
        # Need to fix a comparison between integer and pointer error.
        # Only been able to trace the first one.
        'parest_r': {
            'name': '510.parest_r',
            'links': [],
            'trace_func': '',
            'lang': 'CPP',
        },

        # Does not work with ellcc as it uses linux header.
        # Does not throw.
        'xalancbmk_r': {
            'name': '523.xalancbmk_r',
            'links': [],
            'trace_func': 'xercesc_2_7::ValueStore::contains',
            'trace_func': Benchmark.ROI_FUNC_SEPARATOR.join([
                'xercesc_2_7::ValueStore::contains',
                'xercesc_2_7::NameDatatypeValidator::compare',
            ]),
            'lang': 'CPP',
        },
        'xalancbmk_s': {
            'name': '623.xalancbmk_s',
            'links': [],
            'trace_func': 'xercesc_2_7::ValueStore::contains',
            'lang': 'CPP',
        },

        # Portablity issue with using std::isfinite but include <math.h>, not <cmath>
        # Does not throw.
        # Haven't tested with ellcc.
        'blender_r': {
            'name': '526.blender_r',
            'links': [],
            'trace_func': '',
            'lang': 'CPP',
        },


        # Not working so far due to setjmp/longjmp.
        'omnetpp_s': {
            'name': '620.omnetpp_s',
            'links': [],
            'trace_func': '',
            'lang': 'CPP',
        },

    }

    def __init__(self, benchmark_args):
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
                    benchmark_args=benchmark_args,
                    name=name,
                    target=target,
                    links=links,
                    trace_func=trace_func,
                    lang=lang,
                )
            )
        self.benchmarks.sort(key=lambda x: x.get_name())

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
