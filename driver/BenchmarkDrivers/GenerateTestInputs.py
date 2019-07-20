
from Benchmark import Benchmark
from Benchmark import BenchmarkArgs

from Utils import TransformManager
from Utils import Gem5ConfigureManager

import Constants as C
import Util
import JobScheduler

import os


class TestInputBenchmark(Benchmark):
    def __init__(self, benchmark_args, file):
        self.cwd = os.getcwd()
        self.work_path, self.source = os.path.split(file)
        self.benchmark_name = self.source[0:-2]

        super(TestInputBenchmark, self).__init__(benchmark_args)

    def get_name(self):
        return self.benchmark_name

    def get_links(self):
        return []

    def get_args(self):
        return []

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

    def compile(self, source, flags, defines, includes):
        compiler = C.CC if source.endswith('.c') else C.CXX
        bc = source + '.bc'
        self.debug('Compiling {source} to {bc}.'.format(source=source, bc=bc))
        compile_cmd = [
            compiler,
            source,
            '-c',
            '-emit-llvm',
            '-o',
            bc,
        ]
        for flag in flags:
            compile_cmd.append('-{flag}'.format(flag=flag))
        for define in defines:
            if defines[define] is not None:
                compile_cmd.append(
                    '-D{DEFINE}={VALUE}'.format(DEFINE=define, VALUE=defines[define]))
            else:
                compile_cmd.append('-D{DEFINE}'.format(DEFINE=define))
        for include in includes:
            compile_cmd.append('-I{INCLUDE}'.format(INCLUDE=include))
        Util.call_helper(compile_cmd)
        Util.call_helper([C.OPT, '-instnamer', bc, '-o', bc])
        Util.call_helper([C.LLVM_DIS, bc])
        return bc

    def build_raw_bc(self):
        self.debug('Build raw bitcode.')
        os.chdir(self.work_path)

        bc = self.compile(
            source=self.source,
            flags=['O1'],
            defines=[],
            includes=[],
        )

        raw_bc = self.get_raw_bc()
        Util.call_helper(['mv', bc, raw_bc])

        os.chdir(self.cwd)

    def trace(self):
        os.chdir(self.work_path)
        self.build_trace(
            link_stdlib=False,
            trace_reachable_only=False,
            # debugs=['TracePass']
        )
        # Remember to set the flag to trace traced function.
        os.putenv('LLVM_TDG_WORK_MODE', str(2))
        os.putenv('LLVM_TDG_MEASURE_IN_TRACE_FUNC', 'TRUE')
        self.run_trace(self.get_name())
        os.chdir(self.cwd)

    def clean(self):
        os.chdir(self.work_path)
        Util.call_helper([
            'rm',
            self.get_trace_bc(),
            self.get_trace_bin(),
        ])
        os.chdir(self.cwd)


class TestInputSuite:

    def __init__(self, benchmark_args):

        # Find all the test input c files.
        test_folder = os.path.join(C.LLVM_TDG_BUILD_DIR, 'test')
        test_inputs = list()
        for root, dirs, files in os.walk(test_folder):
            for f in files:
                if f.endswith('.c'):
                    test_inputs.append(os.path.join(root, f))
        self.benchmarks = [TestInputBenchmark(
            benchmark_args, f) for f in test_inputs]

    def get_benchmarks(self):
        return self.benchmarks


def build(benchmark):
    benchmark.build_raw_bc()


def trace(benchmark):
    benchmark.trace()


def clean(benchmark):
    benchmark.clean()


if __name__ == '__main__':

    # Create a fake BenchmarkArgs
    transform_manager = TransformManager.TransformManager([])
    benchmark_args = BenchmarkArgs(
        transform_manager,
        Gem5ConfigureManager.Gem5ReplayConfigureManager([], transform_manager),
        None)
    suite = TestInputSuite(benchmark_args)
    benchmarks = suite.get_benchmarks()
    job_scheduler = JobScheduler.JobScheduler('test', 8, 1)
    for b in benchmarks:
        benchmark_name = b.get_name()
        build_job_id = job_scheduler.add_job(
            benchmark_name + '.build',
            build,
            (b, ),
            list(),
        )
        trace_job_id = job_scheduler.add_job(
            benchmark_name + '.trace',
            trace,
            (b, ),
            [build_job_id]
        )
        # trace_job_id = job_scheduler.add_job(
        #     benchmark_name + '.clean',
        #     clean,
        #     (b, ),
        #     [trace_job_id]
        # )
    job_scheduler.run()
