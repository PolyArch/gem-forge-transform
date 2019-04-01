import os

from Benchmark import Benchmark
import Util
import Constants as C


class Graph500Benchmark(Benchmark):

    TRACE_FUNC = 'make_bfs_tree'
    SCALE = '14'

    def __init__(self, test_name):
        self.cwd = os.getcwd()
        self.work_path = (
            '/home/zhengrong/Documents/graph500/'
        )
        self.test_name = test_name

        super(Graph500Benchmark, self).__init__(
            name=self.get_name(),
            raw_bc=self.get_raw_bc(),
            links=['-lm', '-lrt'],
            args=[
                '-s', Graph500Benchmark.SCALE, '-o', 'test.graph', '-r', 'test.root'],
            trace_func=Graph500Benchmark.TRACE_FUNC,
            lang='C',
        )

    def get_name(self):
        return 'graph500.{test}'.format(test=self.test_name)

    def get_run_path(self):
        return self.work_path

    def get_trace_ids(self):
        return [0]

    def get_raw_bc(self):
        return os.path.join(
            self.work_path,
            self.test_name,
            '{name}.bc'.format(name=self.get_name()),
        )

    def build_raw_bc(self):
        os.chdir(self.work_path)
        Util.call_helper(['make', 'clean'])
        Util.call_helper([
            'make',
        ])
        Util.call_helper([
            './make-edgelist',
            '-s',
            Graph500Benchmark.SCALE,
            '-o',
            'test.graph',
            '-r',
            'test.root'
        ])
        binary = os.path.join(
            self.work_path,
            self.test_name,
            self.test_name
        )
        os.putenv('LLVM_COMPILER_PATH', C.LLVM_BIN_PATH)
        Util.call_helper([
            'get-bc',
            '-o',
            self.get_raw_bc(),
            binary,
        ])
        Util.call_helper([
            C.OPT,
            '-instnamer',
            self.get_raw_bc(),
            '-o',
            self.get_raw_bc()
        ])
        os.chdir(self.cwd)

    def trace(self):
        os.chdir(self.work_path)
        self.build_trace(
            link_stdlib=False,
            trace_reachable_only=False,
        )
        os.putenv('SKIP_VALIDATION', '')
        self.run_trace(self.get_name())
        os.unsetenv('SKIP_VALIDATION')
        os.chdir(self.cwd)


class Graph500Benchmarks:
    def __init__(self):
        self.benchmarks = [
            Graph500Benchmark('seq-list'),
            Graph500Benchmark('seq-csr'),
        ]

    def get_benchmarks(self):
        return self.benchmarks
