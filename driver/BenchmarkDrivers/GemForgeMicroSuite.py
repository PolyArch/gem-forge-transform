
from Benchmark import Benchmark
from Benchmark import BenchmarkArgs

from Utils import TransformManager
from Utils import Gem5ConfigureManager

import Constants as C
import Util

import os


class GemForgeMicroBenchmark(Benchmark):
    def __init__(self, benchmark_args, src_path):
        self.cwd = os.getcwd()
        self.src_path = src_path
        self.benchmark_name = os.path.basename(self.src_path)
        self.source = self.benchmark_name + '.c'

        # Create the result dir out of the source tree.
        self.cwd = os.getcwd()
        self.work_path = os.path.join(
            C.LLVM_TDG_RESULT_DIR, 'gfm', self.benchmark_name
        )
        Util.mkdir_chain(self.work_path)

        super(GemForgeMicroBenchmark, self).__init__(benchmark_args)

    def get_name(self):
        return 'gfm.{b}'.format(b=self.benchmark_name)

    def get_links(self):
        return ['-lm']

    def get_args(self):
        return None

    def get_trace_func(self):
        return 'foo'

    def get_lang(self):
        return 'C'

    def get_raw_bc(self):
        return '{name}.bc'.format(name=self.get_name())

    def get_exe_path(self):
        return self.work_path

    def get_run_path(self):
        return self.work_path

    def build_raw_bc(self):
        os.chdir(self.src_path)
        bc = os.path.join(self.work_path, self.get_raw_bc())
        compile_cmd = [
            C.CC,
            self.source,
            '-O3',
            '-c',
            '-fno-unroll-loops',
            '-fno-vectorize',
            '-fno-slp-vectorize',
            '-emit-llvm',
            '-std=c99',
            '-gline-tables-only',
            '-I{INCLUDE}'.format(INCLUDE=os.path.dirname(self.src_path)),
            '-o',
            bc
        ]
        Util.call_helper(compile_cmd)
        # Remember to name every instruction.
        Util.call_helper([C.OPT, '-instnamer', bc, '-o', bc])
        Util.call_helper([C.LLVM_DIS, bc])

        os.chdir(self.cwd)

    def trace(self):
        os.chdir(self.work_path)
        self.build_trace(
            link_stdlib=False,
            trace_reachable_only=False,
        )
        # For this benchmark, we only trace the target function.
        os.putenv('LLVM_TDG_WORK_MODE', str(2))
        os.putenv('LLVM_TDG_MEASURE_IN_TRACE_FUNC', 'TRUE')
        self.run_trace(self.get_name())
        os.chdir(self.cwd)

    def transform(self, transform_config, trace, profile_file, tdg, debugs):
        os.chdir(self.work_path)

        self.build_replay(
            transform_config=transform_config,
            trace=trace,
            profile_file=profile_file,
            tdg_detail='standalone',
            output_tdg=tdg,
            debugs=debugs,
        )

        os.chdir(self.cwd)


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
