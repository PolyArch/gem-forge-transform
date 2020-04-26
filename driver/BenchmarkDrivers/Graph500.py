
from Benchmark import Benchmark
from Benchmark import BenchmarkArgs

from Utils import TraceFlagEnum

import Constants as C
import Util

import os


class Graph500Benchmark(Benchmark):

    def __init__(self, benchmark_args, src_path):
        self.cwd = os.getcwd()
        self.src_path = src_path
        self.suite_path = os.path.dirname(self.src_path)
        self.benchmark_name = os.path.basename(self.src_path)
        self.n_thread = benchmark_args.options.input_threads

        # Create the result dir out of the source tree.
        self.work_path = os.path.join(
            C.LLVM_TDG_RESULT_DIR, 'graph500', self.benchmark_name
        )
        Util.mkdir_chain(self.work_path)

        self.scale = 16
        self.edge_factor = 16
        self.edge_list_file = 's{s}-e{e}.graph'.format(s=self.scale, e=self.edge_factor)
        self.root_file = 's{s}-e{e}.root'.format(s=self.scale, e=self.edge_factor)
        self.edge_list_file = os.path.join(self.work_path, self.edge_list_file)
        self.root_file = os.path.join(self.work_path, self.root_file)
        self.csr_file = self.edge_list_file + '.csr'

        super(Graph500Benchmark, self).__init__(benchmark_args)

    def get_name(self):
        return 'graph.{b}'.format(b=self.benchmark_name)

    def get_raw_bc(self):
        # We have a special raw bc name.
        return self.get_name() + '.bc'

    def get_links(self):
        links = ['-lm', '-lrt']
        if self.benchmark_name == 'omp-csr':
            links += [
                '-lomp',
                '-lpthread',
                '-Wl,--no-as-needed',
                '-ldl',
            ]
        return links

    def get_args(self):
        return [
            '-s',
            str(self.scale),
            '-e',
            str(self.edge_factor),
            '-o',
            self.edge_list_file,
            '-r',
            self.root_file,
            '-t',
            str(self.n_thread),
            '-z',
            '-V',
        ]

    def get_trace_func(self):
        if self.benchmark_name == 'omp-csr':
            return '.omp_outlined.'
        return 'make_bfs_tree'

    def get_lang(self):
        return 'C'

    def get_exe_path(self):
        return self.work_path

    def get_run_path(self):
        return self.work_path

    def generate_input(self):
        if os.path.isfile(self.edge_list_file):
            return
        Util.call_helper([
            'make',
            'make-edgelist',
        ])
        # Put the edge list in the work path.
        Util.call_helper([
            './make-edgelist',
            '-s',
            str(self.scale),
            '-e',
            str(self.edge_factor),
            '-o',
            self.edge_list_file,
            '-r',
            self.root_file,
        ])
    def generate_csr(self):
        # Generate the intermediate file for fast simulation (only for omp-csr).
        if self.benchmark_name != 'omp-csr':
            return
        if os.path.isfile(self.csr_file):
            return
        Util.call_helper([
            'make',
            'omp-csr/omp-csr'
        ])
        Util.call_helper([
            './omp-csr/omp-csr',
            '-s',
            str(self.scale),
            '-e',
            str(self.edge_factor),
            '-o',
            self.edge_list_file,
            '-r',
            self.root_file,
        ])

    def build_raw_bc(self):
        # First we make the edge list.
        os.chdir(self.suite_path)
        # Generate the input.
        self.generate_input()
        self.generate_csr()
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

    def run_profile(self):
        # Simply set the ROI env before we profile as we only care
        # about the specified function.
        os.putenv('LLVM_TDG_TRACE_ROI', str(
            TraceFlagEnum.GemForgeTraceROI.SpecifiedFunction.value
        ))
        super(Graph500Benchmark, self).run_profile()
        os.unsetenv('LLVM_TDG_TRACE_ROI')

    def get_additional_transform_options(self):
        """
        Add the stream whitelist file as addition options to the transformation.
        """
        if self.benchmark_name == 'seq-list':
            return [
                '-stream-whitelist-file={whitelist}'.format(whitelist=os.path.join(
                    self.src_path, 'stream_whitelist.txt'
                ))
            ]
        if self.benchmark_name == 'seq-csr':
            return [
                '-stream-whitelist-file={whitelist}'.format(whitelist=os.path.join(
                    self.src_path, 'stream_whitelist.txt'
                ))
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
            TraceFlagEnum.GemForgeTraceMode.TraceSpecifiedInterval.value))
        os.putenv('LLVM_TDG_INTERVALS_FILE', self.get_simpoint_abs())
        os.putenv('LLVM_TDG_TRACE_ROI', str(
            TraceFlagEnum.GemForgeTraceROI.SpecifiedFunction.value))
        self.run_trace()
        os.chdir(self.cwd)

    def get_additional_gem5_simulate_command(self):
        """
        We just simulate one BFS.
        """
        n_bfs = 1
        if n_bfs == -1:
            # This benchmark can finish.
            return list()
        return [
            '--work-end-exit-count={v}'.format(v=n_bfs)
        ]


class Graph500Benchmarks:
    def __init__(self, benchmark_args):
        suite_folder = os.getenv('GRAPH500_SUITE_PATH')
        self.benchmarks = list()
        for name in [
            # 'seq-list',
            # 'seq-csr',
            'omp-csr',
        ]:
            src_path = os.path.join(suite_folder, name)
            self.benchmarks.append(
                Graph500Benchmark(benchmark_args, src_path)
            )

    def get_benchmarks(self):
        return self.benchmarks
