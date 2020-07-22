
from Benchmark import Benchmark
from Benchmark import BenchmarkArgs

from Utils import TransformManager
from Utils import Gem5ConfigureManager
from Utils import TraceFlagEnum

import Constants as C
import Util

import os


class ParsecBenchmark(Benchmark):

    # Hardcode the input arguments.
    ARGS = {
        'blackscholes': {
            'test': ['$NTHREADS', 'in_4.txt', 'prices.txt'],
            'simdev': ['$NTHREADS', 'in_16.txt', 'prices.txt'],
            'simsmall': ['$NTHREADS', 'in_4K.txt', 'prices.txt'],
            'simmedium': ['$NTHREADS', 'in_16K.txt', 'prices.txt'],
            'simlarge': ['$NTHREADS', 'in_64K.txt', 'prices.txt'],
            'native': ['$NTHREADS', 'in_10M.txt', 'prices.txt'],
        },
        'bodytrack': {
            'test': ['sequenceB_1', '4', '1', '5', '1', '0', '$NTHREADS'],
            'simdev': ['sequenceB_1', '4', '1', '100', '3', '0', '$NTHREADS'],
            'simsmall': ['sequenceB_1', '4', '1', '1000', '5', '0', '$NTHREADS'],
            'simmedium': ['sequenceB_2', '4', '2', '2000', '5', '0', '$NTHREADS'],
            'simlarge': ['sequenceB_4', '4', '4', '4000', '5', '0', '$NTHREADS'],
            'native': ['sequenceB_261', '4', '261', '4000', '5', '0', '$NTHREADS'],
        },
        'canneal': {
            'test':      ['$NTHREADS', '5', '100', '10.nets', '1'],
            'simdev':    ['$NTHREADS', '100', '300', '100.nets', '2'],
            'simsmall':  ['$NTHREADS', '10000', '2000', '100000.nets', '32'],
            'simmedium': ['$NTHREADS', '15000', '2000', '200000.nets', '64'],
        },
        'fluidanimate': {
            'test':      ['$NTHREADS', '1', 'in_5K.fluid',  ],
            'simdev':    ['$NTHREADS', '3', 'in_15K.fluid', ],
            'simsmall':  ['$NTHREADS', '5', 'in_35K.fluid', ],
            'simmedium': ['$NTHREADS', '1', 'in_100K.fluid',],
            'simlarge': ['$NTHREADS', '5', 'in_300K.fluid', ],
        }
    }

    LEGAL_INPUT_SIZE = {'test', 'simdev',
                        'simsmall', 'simmedium', 'simlarge', 'native'}

    def __init__(self, benchmark_args, benchmark_path):
        self.cwd = os.getcwd()
        self.benchmark_path = benchmark_path
        self.obj_path = os.path.join(
            self.benchmark_path, 'obj/amd64-linux.clang-pthreads')
        self.hook_lib_path = os.path.join(
            os.path.dirname(os.path.dirname(self.benchmark_path)),
            'libs/hooks/inst/amd64-linux.clang-pthreads/lib'
        )

        self.input_name = 'test'
        # Override the input_name if use-specified.
        if benchmark_args.options.input_name:
            self.input_name = benchmark_args.options.input_name
        assert(self.input_name in ParsecBenchmark.LEGAL_INPUT_SIZE)
        self.sim_input_name = 'test'
        if benchmark_args.options.sim_input_name:
            self.sim_input_name = benchmark_args.options.sim_input_name
        assert(self.sim_input_name in ParsecBenchmark.LEGAL_INPUT_SIZE)

        self.exe_path = os.path.join(
            self.benchmark_path, 'run', self.input_name)
        self.setup_input(self.exe_path, self.input_name)
        self.sim_exe_path = os.path.join(
            self.benchmark_path, 'run', self.sim_input_name)
        self.setup_input(self.sim_exe_path, self.sim_input_name)

        self.benchmark_name = os.path.basename(self.benchmark_path)
        self.n_thread = benchmark_args.options.input_threads
        if self.benchmark_name == 'bodytrack':
            self.n_thread = max(1, self.n_thread - 2)
        elif self.benchmark_name == 'fluidanimate':
            self.n_thread = max(1, self.n_thread - 1)

        # Create the result dir out of the source tree.
        self.work_path = os.path.join(
            C.LLVM_TDG_RESULT_DIR, 'parsec', self.benchmark_name
        )
        Util.mkdir_chain(self.work_path)

        super(ParsecBenchmark, self).__init__(benchmark_args)

    def setup_input(self, exe_path, input_name):
        if os.path.isfile(exe_path):
            # Exe path is not a folder.
            assert(False)
        Util.mkdir_p(exe_path)
        os.chdir(exe_path)
        input_ready_file = 'input_ready_{size}'.format(size=input_name)
        if os.path.isfile(input_ready_file):
            # We assume the input is there.
            return
        input_tar = os.path.join(
            self.benchmark_path, 'inputs', 'input_{size}.tar'.format(size=input_name))
        untar_cmd = [
            'tar',
            'xvf',
            input_tar,
        ]
        Util.call_helper(untar_cmd)
        Util.call_helper(['touch', input_ready_file])
        os.chdir(self.cwd)

    def get_name(self):
        return 'parsec.{b}'.format(b=self.benchmark_name)

    def get_input_name(self):
        return self.input_name

    def get_sim_input_name(self):
        return self.sim_input_name

    def get_links(self):
        return [
            '-lm',
            '-lpthread',
        ]
    
    def get_gem5_links(self):
        if C.M5_THREADS_LIB is None:
            print('Please define M5_THREADS_LIB.')
            assert(False)
        return [
            '-lm',
            C.M5_THREADS_LIB,
        ]

    def _get_args(self, input_name):
        args = list()
        for arg in ParsecBenchmark.ARGS[self.benchmark_name][input_name]:
            if arg == '$NTHREADS':
                args.append(str(self.n_thread))
            else:
                args.append(arg)
        return args

    def get_args(self):
        return self._get_args(self.input_name)

    def get_sim_args(self):
        return self._get_args(self.sim_input_name)

    def get_trace_func(self):
        if self.benchmark_name == 'blackscholes':
            return 'bs_thread'
        elif self.benchmark_name == 'bodytrack':
            return 'ParticleFilterPthread<TrackingModel>::Exec'
        elif self.benchmark_name == 'canneal':
            return 'annealer_thread::Run()'
        elif self.benchmark_name == 'fluidanimate':
            return 'AdvanceFrameMT'
        return None

    def get_lang(self):
        return 'CPP'

    def get_exe_path(self):
        return self.exe_path

    def get_sim_exe_path(self):
        return self.sim_exe_path

    def get_run_path(self):
        return self.work_path

    def get_profile_roi(self):
        return TraceFlagEnum.GemForgeTraceROI.SpecifiedFunction.value

    def build_raw_bc(self):
        uninstall_cmd = [
            'parsecmgmt',
            '-a',
            'uninstall',
            '-c',
            'clang-pthreads',
            '-p',
            self.benchmark_name,
        ]
        Util.call_helper(uninstall_cmd)
        build_cmd = [
            'parsecmgmt',
            '-a',
            'build',
            '-c',
            'clang-pthreads',
            '-p',
            self.benchmark_name,
        ]
        Util.call_helper(build_cmd)
        os.chdir(self.obj_path)
        # Rename the bytecode.
        mv_cmd = [
            'mv',
            '{b}.0.5.precodegen.bc'.format(b=self.benchmark_name),
            self.get_raw_bc(),
        ]
        print(os.getcwd())
        Util.call_helper(mv_cmd)
        # Remember to name every instruction.
        Util.call_helper(
            [C.OPT, '-instnamer', self.get_raw_bc(), '-o', self.get_raw_bc()])
        # Strip away the m5 pseudo instructions.
        Util.call_helper([
            C.OPT,
            '-load={PASS_SO}'.format(PASS_SO=self.pass_so),
            '-strip-m5-pass',
            self.get_raw_bc(),
            '-o',
            self.get_raw_bc(),
        ])
        # Copy to work_path
        cp_cmd = [
            'cp',
            self.get_raw_bc(),
            self.get_run_path(),
        ]
        Util.call_helper(cp_cmd)

        os.chdir(self.cwd)

    def get_additional_transform_options(self):
        return list()

    def trace(self):
        # Copy the bc to the exe path.
        Util.call_helper([
            'cp',
            '-f',
            os.path.join(self.get_run_path(), self.get_raw_bc()),
            os.path.join(self.get_exe_path(), self.get_raw_bc()),
        ])
        os.chdir(self.get_exe_path())
        self.build_trace(
            link_stdlib=False,
            trace_reachable_only=False,
        )

        # For this benchmark, we only trace the target function.
        os.putenv('LLVM_TDG_TRACE_MODE', str(
            TraceFlagEnum.GemForgeTraceMode.TraceAll.value))
        os.putenv('LLVM_TDG_TRACE_ROI', str(
            TraceFlagEnum.GemForgeTraceROI.SpecifiedFunction.value))
        # So for we just trace 10 million to get a fake trace so that valid.ex can run.
        if self.benchmark_name == 'fluidanimate':
            os.putenv('LLVM_TDG_HARD_EXIT_IN_MILLION', str(10))
        self.run_trace()

        os.chdir(self.cwd)

    def build_validation(self, transform_config, trace, output_tdg):
        # Build the validation binary.
        output_dir = os.path.dirname(output_tdg)
        binary = os.path.join(output_dir, self.get_valid_bin())
        build_cmd = [
            C.CC,
            '-static',
            '-O3',
            self.get_raw_bc(),
            # Link with gem5.
            '-I{gem5_include}'.format(gem5_include=C.GEM5_INCLUDE_DIR),
            C.GEM5_M5OPS_X86,
            '-lm',
            '-o',
            binary
        ]
        Util.call_helper(build_cmd)
        asm = os.path.join(output_dir, self.get_name() + '.asm')
        with open(asm, 'w') as f:
            disasm_cmd = [
                'objdump',
                '-d',
                binary
            ]
            Util.call_helper(disasm_cmd, stdout=f)

    def simulate_valid(self, tdg, transform_config, simulation_config):
        print("# Simulating the validation binary.")
        gem5_out_dir = simulation_config.get_gem5_dir(tdg)
        tdg_dir = os.path.dirname(tdg)
        binary = os.path.join(tdg_dir, self.get_valid_bin())
        gem5_args = self.get_gem5_simulate_command(
            simulation_config=simulation_config,
            binary=binary,
            outdir=gem5_out_dir,
            standalone=False,
        )
        # Exit immediately when we are done.
        gem5_args.append('--work-end-exit-count=1')
        # Do not add the fake tdg file, so that the script in gem5 will
        # actually simulate the valid bin.
        Util.call_helper(gem5_args)


class ParsecSuite:
    def __init__(self, benchmark_args):
        suite_folder = os.getenv('PARSEC_SUITE_PATH')
        # Every folder in the suite is a benchmark.
        self.benchmarks = list()
        sub_folders = ['pkgs/apps', 'pkgs/kernels']
        for sub_folder in sub_folders:
            items = os.listdir(os.path.join(suite_folder, sub_folder))
            items.sort()
            for item in items:
                if item[0] == '.':
                    # Ignore special folders.
                    continue
                benchmark_name = 'parsec.{b}'.format(b=os.path.basename(item))
                if benchmark_name not in benchmark_args.options.benchmark:
                    # Ignore benchmark not required.
                    continue
                abs_path = os.path.join(suite_folder, sub_folder, item)
                if os.path.isdir(abs_path):
                    self.benchmarks.append(
                        ParsecBenchmark(benchmark_args, abs_path))

    def get_benchmarks(self):
        return self.benchmarks


"""
Parsec Kernel Function.

==========================================================
Blackscholes: 
Works with RISCV.
100% bs_thread().
Simple data-parallel float-intensive.
Linear access.

==========================================================
Bodytrack:
Works with RISCV.
Somehow I still feel this is compute intensive.
18% void MultiCameraProjectedBody::ImageProjection()
33% float ImageMeasurements::ImageErrorEdge() 
40% float ImageMeasurements::ImageErrorInside()

==========================================================
Canneal: annealer_thread.
Not work with RISCV due to assembly for atomic operation.

==========================================================
Fluidanimate: AdvanceFrameMT(int)
47% ComputeForcesMT
45% ComputeDensitiesMT

==========================================================
x264:
Kernel is assembly code.
Double free, segment fault even in native machine.

Ferret:
Works in native machine.
Failed in gem5 due to unimplemented syscall msync (try m5 thread).

Swaptions:
Works in native kilia.

streamcluster:
Works in native machine.


"""
