
from Benchmark import Benchmark
from Benchmark import BenchmarkArgs

import Constants as C
import Util

import os


class Graph500Benchmark(Benchmark):

    def __init__(self, benchmark_args, src_path):
        self.cwd = os.getcwd()
        self.src_path = src_path
        self.suite_path = os.path.dirname(self.src_path)
        self.benchmark_name = os.path.basename(self.src_path)
        self.work_path = (
            '/home/zhengrong/Documents/graph500/'
        )

        # Create the result dir out of the source tree.
        self.work_path = os.path.join(
            C.LLVM_TDG_RESULT_DIR, 'graph500', self.benchmark_name
        )
        Util.mkdir_chain(self.work_path)

        self.scale = 14
        self.edge_list_file = 'test.graph'
        self.root_file = 'test.root'

        super(Graph500Benchmark, self).__init__(benchmark_args)

    def get_name(self):
        return 'graph.{b}'.format(b=self.benchmark_name)

    def get_links(self):
        return ['-lm', '-lrt']

    def get_args(self):
        return [
            '-s',
            str(self.scale),
            '-o',
            self.edge_list_file,
            '-r',
            self.root_file,
        ]

    def get_trace_func(self):
        return 'make_bfs_tree'

    def get_lang(self):
        return 'C'

    def get_raw_bc(self):
        return '{name}.bc'.format(name=self.get_name())

    def get_exe_path(self):
        return self.work_path

    def get_run_path(self):
        return self.work_path

    def build_raw_bc(self):
        # First we make the edge list.
        os.chdir(self.suite_path)
        Util.call_helper(['make', 'clean'])
        Util.call_helper([
            'make',
            'make-edgelist',
        ])
        # Put the edge list in the work path.
        Util.call_helper([
            './make-edgelist',
            '-s',
            str(self.scale),
            '-o',
            os.path.join(self.work_path, self.edge_list_file),
            '-r',
            os.path.join(self.work_path, self.root_file),
        ])
        # Make the bc using the special target.
        Util.call_helper([
            'make',
            self.get_raw_bc(),
        ])
        # Simpy move the bc to the work path.
        Util.call_helper([
            'mv',
            self.get_raw_bc(),
            os.path.join(self.work_path, self.get_raw_bc()),
        ])
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


class Graph500Benchmarks:
    def __init__(self, benchmark_args):
        suite_folder = os.getenv('GRAPH500_SUITE_PATH')
        self.benchmarks = list()
        for name in [
            'seq-list',
            'seq-csr',
        ]:
            src_path = os.path.join(suite_folder, name)
            self.benchmarks.append(
                Graph500Benchmark(benchmark_args, src_path)
            )

    def get_benchmarks(self):
        return self.benchmarks
