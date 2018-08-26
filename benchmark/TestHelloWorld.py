
from Benchmark import Benchmark
import Util
import StreamStatistics
import Constants as C

import os


class TestHelloWorldBenchmark(Benchmark):
    def __init__(self):
        super(TestHelloWorldBenchmark, self).__init__(
            name='hello.a', raw_bc='all.bc', links=[])

        self.work_path = os.path.join(
            C.LLVM_TDG_DIR,
            'benchmark',
            'test',
        )

        self.cwd = os.getcwd()

    def get_name(self):
        return 'hello.a'

    def get_run_path(self):
        return self.work_path

    def get_n_traces(self):
        return 1

    def get_raw_bc(self):
        return 'a.bc'

    def build_raw_bc(self):
        os.chdir(self.work_path)

        Util.call_helper([
            'clang',
            '-emit-llvm',
            '-S',
            '-O1',
            '-fno-inline-functions',
            '-fno-vectorize',
            '-fno-slp-vectorize',
            '-fno-unroll-loops',
            'a.c',
            'dump.c',
        ])

        Util.call_helper([
            'llvm-link',
            'a.ll',
            'dump.ll',
            '-o',
            'all.bc'
        ])

        Util.call_helper([
            'opt',
            '-instnamer',
            'all.bc',
            '-o',
            'all.bc',
        ])

        Util.call_helper([
            'llvm-dis',
            'all.bc',
        ])
        os.chdir(self.cwd)

    def trace(self):
        os.chdir(self.work_path)
        self.build_trace()
        self.run_trace(self.get_name())
        os.chdir(self.cwd)

    def transform(self, pass_name, trace, profile_file, tdg, debugs):
        os.chdir(self.work_path)

        self.build_replay(
            pass_name=pass_name,
            trace_file=trace,
            profile_file=profile_file,
            # tdg_detail='integrated',
            tdg_detail='standalone',
            output_tdg=tdg,
            debugs=debugs,
        )

        os.chdir(self.cwd)

    def simulate(self, tdg, result, debugs):
        os.chdir(self.work_path)
        gem5_outdir = self.gem5_replay(
            standalone=1,
            output_tdg=tdg,
            debugs=debugs,
        )
        Util.call_helper([
            'cp',
            # os.path.join(gem5_outdir, 'stats.txt'),
            os.path.join(gem5_outdir, 'region.stats.txt'),
            result,
        ])
        os.chdir(self.cwd)

class TestHelloWorldBenchmarks:
    def __init__(self):
        self.benchmarks = [
            TestHelloWorldBenchmark()
        ]

    def get_benchmarks(self):
        return self.benchmarks