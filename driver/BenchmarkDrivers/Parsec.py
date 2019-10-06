
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
        },
        'bodytrack': {
            'test': ['sequenceB_1', '4', '1', '5', '1', '0', '$NTHREADS'],
            'simdev': ['sequenceB_1', '4', '1', '100', '3', '0', '$NTHREADS'],
            'simsmall': ['sequenceB_1', '4', '1', '1000', '5', '0', '$NTHREADS'],
            'simmedium': ['sequenceB_2', '4', '2', '2000', '5', '0', '$NTHREADS'],
        },
        'canneal': {
            'test':      ['$NTHREADS', '5', '100', '10.nets', '1'],
            'simdev':    ['$NTHREADS', '100', '300', '100.nets', '2'],
            'simsmall':  ['$NTHREADS', '10000', '2000', '100000.nets', '32'],
            'simmedium': ['$NTHREADS', '15000', '2000', '200000.nets', '64'],
        },
    }

    def __init__(self, benchmark_args, benchmark_path):
        self.cwd = os.getcwd()
        self.benchmark_path = benchmark_path
        self.obj_path = os.path.join(
            self.benchmark_path, 'obj/amd64-linux.clang-pthreads')
        self.hook_lib_path = os.path.join(
            os.path.dirname(os.path.dirname(self.benchmark_path)),
            'libs/hooks/inst/amd64-linux.clang-pthreads/lib'
        )

        self.input_size = 'test'
        # Override the input_size if use-specified.
        if benchmark_args.options.input_size:
            self.input_size = benchmark_args.options.input_size
        assert(self.input_size in {'test', 'simdev',
                                   'simsmall', 'simmedium', 'simlarge', 'native'})

        self.exe_path = os.path.join(
            self.benchmark_path, 'run', self.input_size)
        self.setup_input()

        self.benchmark_name = os.path.basename(self.benchmark_path)
        self.n_thread = 1

        # Create the result dir out of the source tree.
        self.work_path = os.path.join(
            C.LLVM_TDG_RESULT_DIR, 'parsec', self.benchmark_name
        )
        Util.mkdir_chain(self.work_path)

        super(ParsecBenchmark, self).__init__(benchmark_args)

    def setup_input(self):
        if os.path.isdir(self.exe_path):
            # We assume the input is there.
            return
        elif os.path.isfile(self.exe_path):
            # Exe path is not a folder.
            assert(False)
        Util.mkdir_p(self.exe_path)
        input_tar = os.path.join(
            self.benchmark_path, 'inputs', 'input_{size}.tar'.format(size=self.input_size))
        os.chdir(self.exe_path)
        untar_cmd = [
            'tar',
            'xvf',
            input_tar,
        ]
        Util.call_helper(untar_cmd)
        os.chdir(self.cwd)

    def get_name(self):
        return 'parsec.{b}'.format(b=self.benchmark_name)

    def get_links(self):
        return [
            '-lm',
            '-lpthread',
        ]

    def get_args(self):
        args = list()
        for arg in ParsecBenchmark.ARGS[self.benchmark_name][self.input_size]:
            if arg == '$NTHREADS':
                args.append(str(self.n_thread))
            else:
                args.append(arg)
        return args

    def get_trace_func(self):
        if self.benchmark_name == 'blackscholes':
            return 'bs_thread(void*)'
        elif self.benchmark_name == 'bodytrack':
            return 'ParticleFilterPthread<TrackingModel>::Exec(unsigned short, unsigned int)'
        elif self.benchmark_name == 'canneal':
            return 'annealer_thread::Run()'
        return None

    def get_lang(self):
        return 'CPP'

    def get_raw_bc(self):
        return '{name}.bc'.format(name=self.get_name())

    def get_exe_path(self):
        return self.exe_path

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
        self.run_trace(self.get_name())

        os.chdir(self.cwd)

    # def get_additional_gem5_simulate_command(self):
    #     # For validation, we disable cache warm.
    #     return ['--gem-forge-cold-cache']

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

Blackscholes: 
100% bs_thread().
Simple data-parallel float-intensive.
Linear access.

Bodytrack:
Somehow I still feel this is compute intensive.
18% void MultiCameraProjectedBody::ImageProjection()
33% float ImageMeasurements::ImageErrorEdge() 
40% float ImageMeasurements::ImageErrorInside()

Canneal: annealer_thread && Assembly for atomic operation.


"""
