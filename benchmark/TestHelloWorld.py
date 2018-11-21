
from Benchmark import Benchmark
import Util
import StreamStatistics
import Constants as C

import os


class TestHelloWorldBenchmark(Benchmark):
    def __init__(self):
        super(TestHelloWorldBenchmark, self).__init__(
            name='hello.a', raw_bc='all.bc', links=[],
            # trace_func='imagick_streams_work',
            # trace_func='xalancbmk_streams_work',
            trace_func='sparse_vec_multi',
            # trace_func='sum2d',
            # trace_func='ivstream_test',
            # trace_func='tri_add',
        )

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

    def get_trace_ids(self):
        return [0]

    def get_raw_bc(self):
        return 'a.bc'

    def build_raw_bc(self):
        os.chdir(self.work_path)

        Util.call_helper([
            C.CC,
            '-emit-llvm',
            '-S',
            '-O1',
            '-fno-inline-functions',
            '-fno-vectorize',
            '-fno-slp-vectorize',
            '-fno-unroll-loops',
            'a.c',
            'dump.c',
            'imagick_streams.c',
            'xalancbmk_streams.c'
        ])

        Util.call_helper([
            C.LLVM_LINK,
            'a.ll',
            'dump.ll',
            'imagick_streams.ll',
            'xalancbmk_streams.ll',
            '-o',
            'all.bc'
        ])

        Util.call_helper([
            C.OPT,
            '-instnamer',
            'all.bc',
            '-o',
            'all.bc',
        ])

        Util.call_helper([
            C.LLVM_DIS,
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


class TestHelloWorldBenchmarks:
    def __init__(self):
        self.benchmarks = [
            TestHelloWorldBenchmark()
        ]

    def get_benchmarks(self):
        return self.benchmarks
