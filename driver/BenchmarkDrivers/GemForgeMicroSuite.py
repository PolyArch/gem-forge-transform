
from Benchmark import Benchmark
from Benchmark import BenchmarkArgs

from Utils import TransformManager
from Utils import Gem5ConfigureManager
from Utils import TraceFlagEnum

import Constants as C
import Util

import os


class GemForgeMicroBenchmark(Benchmark):
    def __init__(self, benchmark_args, src_path):
        self.cwd = os.getcwd()
        self.src_path = src_path
        self.benchmark_name = os.path.basename(self.src_path)
        self.source = self.benchmark_name + '.c'
        self.stream_whitelist_fn = os.path.join(
            self.src_path, 'stream_whitelist.txt')

        # Create the result dir out of the source tree.
        self.work_path = os.path.join(
            C.LLVM_TDG_RESULT_DIR, 'gfm', self.benchmark_name
        )
        Util.mkdir_chain(self.work_path)

        super(GemForgeMicroBenchmark, self).__init__(benchmark_args)

    def get_name(self):
        return 'gfm.{b}'.format(b=self.benchmark_name)

    def get_links(self):
        return []

    def get_args(self):
        return None

    def get_trace_func(self):
        return 'foo'

    def get_lang(self):
        return 'C'

    def get_exe_path(self):
        return self.work_path

    def get_run_path(self):
        return self.work_path

    def get_profile_roi(self):
        return TraceFlagEnum.GemForgeTraceROI.SpecifiedFunction.value

    def build_raw_bc(self):
        os.chdir(self.src_path)
        bc = os.path.join(self.work_path, self.get_raw_bc())
        compile_cmd = [
            C.CC,
            self.source,
            '-O3',
            '-c',
            # '-fno-unroll-loops',
            # '-fno-vectorize',
            # '-fno-slp-vectorize',
            '-emit-llvm',
            '-std=c99',
            '-gline-tables-only',
            '-I{INCLUDE}'.format(INCLUDE=C.GEM5_INCLUDE_DIR),
            '-o',
            bc
        ]
        Util.call_helper(compile_cmd)
        # Remember to name every instruction.
        Util.call_helper([C.OPT, '-instnamer', bc, '-o', bc])
        Util.call_helper([C.LLVM_DIS, bc])

        os.chdir(self.cwd)

    def get_additional_transform_options(self):
        """
        Adhoc stream whitelist file as additional option.
        """
        if os.path.isfile(self.stream_whitelist_fn):
            return [
                '-stream-whitelist-file={whitelist}'.format(
                    whitelist=self.stream_whitelist_fn)
            ]
        return list()

    def trace(self):
        os.chdir(self.work_path)
        self.build_trace(
            link_stdlib=False,
            trace_reachable_only=False,
        )
        # For this benchmark, we only trace the target function.
        os.putenv('LLVM_TDG_TRACE_MODE', str(
            TraceFlagEnum.GemForgeTraceMode.TraceAll.value))
        os.putenv('LLVM_TDG_TRACE_ROI', str(
            TraceFlagEnum.GemForgeTraceROI.SpecifiedFunction.value))
        self.run_trace()
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


class GemForgeMicroSuite:
    def __init__(self, benchmark_args):
        suite_folder = os.path.join(
            C.LLVM_TDG_BENCHMARK_DIR, 'GemForgeMicroSuite')
        # Every folder in the suite is a benchmark.
        items = os.listdir(suite_folder)
        items.sort()
        self.benchmarks = list()
        for item in items:
            if item[0] == '.':
                # Ignore special folders.
                continue
            abs_path = os.path.join(suite_folder, item)
            if os.path.isdir(abs_path):
                self.benchmarks.append(
                    GemForgeMicroBenchmark(benchmark_args, abs_path))

    def get_benchmarks(self):
        return self.benchmarks
