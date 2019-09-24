
from Benchmark import Benchmark
from Benchmark import BenchmarkArgs

from Utils import TransformManager
from Utils import Gem5ConfigureManager
from Utils import TraceFlagEnum

import Constants as C
import Util

import os


class ParsecBenchmark(Benchmark):
    def __init__(self, benchmark_args, benchmark_path):
        self.cwd = os.getcwd()
        self.benchmark_path = benchmark_path
        self.obj_path = os.path.join(
            self.benchmark_path, 'obj/amd64-linux.clang-pthreads')

        self.input_size = 'test'
        # Override the input_size if use-specified.
        if benchmark_args.options.input_size:
            self.input_size = benchmark_args.options.input_size
        assert(self.input_size in {'test'})

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
        return ['-lm', '-lpthread']

    def get_args(self):
        if self.benchmark_name == 'blackscholes':
            return [
                str(self.n_thread),
                'in_4.txt',
                'prices.txt',
            ]
        return None

    def get_trace_func(self):
        if self.benchmark_name == 'blackscholes':
            return 'bs_thread(void*)'
        return None

    def get_lang(self):
        return 'CPP'

    def get_raw_bc(self):
        return '{name}.bc'.format(name=self.get_name())

    def get_exe_path(self):
        return self.exe_path

    def get_run_path(self):
        return self.work_path

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
        # Copy to exe_path
        cp_cmd = [
            'cp',
            self.get_raw_bc(),
            self.exe_path,
        ]
        Util.call_helper(cp_cmd)
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
            trace_reachable_only=True,
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
        sub_folder = 'pkgs/apps'
        items = os.listdir(os.path.join(suite_folder, sub_folder))
        items.sort()
        self.benchmarks = list()
        for item in items:
            if item[0] == '.':
                # Ignore special folders.
                continue
            abs_path = os.path.join(suite_folder, sub_folder, item)
            if os.path.isdir(abs_path):
                self.benchmarks.append(
                    ParsecBenchmark(benchmark_args, abs_path))

    def get_benchmarks(self):
        return self.benchmarks
