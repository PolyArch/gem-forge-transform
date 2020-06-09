
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
        self.graph_utils_source = '../gfm_graph_utils.c'
        self.stream_whitelist_fn = os.path.join(
            self.src_path, 'stream_whitelist.txt')

        self.is_omp = self.benchmark_name.startswith('omp_')
        self.is_graph = self.benchmark_name in [
            'omp_bfs',
            'omp_bfs_queue',
            'omp_page_rank',
        ]
        self.n_thread = benchmark_args.options.input_threads
        self.sim_input_name = benchmark_args.options.sim_input_name

        # Create the result dir out of the source tree.
        self.work_path = os.path.join(
            C.LLVM_TDG_RESULT_DIR, 'gfm', self.benchmark_name
        )
        Util.mkdir_chain(self.work_path)

        super(GemForgeMicroBenchmark, self).__init__(benchmark_args)

    def get_name(self):
        return 'gfm.{b}'.format(b=self.benchmark_name)

    def get_links(self):
        if self.is_omp:
            return [
                '-lomp',
                '-lpthread',
                '-Wl,--no-as-needed',
                '-ldl',
            ]
        return []

    def get_args(self):
        if self.is_omp:
            args = [str(self.n_thread)]
            if self.is_graph:
                graphs = os.path.join(os.getenv('BENCHMARK_PATH'), 'graphs')
                args.append(os.path.join(graphs, '{i}.bin'.format(i=self.sim_input_name)))
            return args
        return None

    def get_sim_input_name(self):
        # Only these workloads has sim_input_name.
        if self.is_graph:
            return self.sim_input_name
        return None

    OMP_GRAPH_FUNC_SUFFIX = {
        'omp_bfs': ['', '.1'],
        'omp_bfs_queue': [''],
        'omp_page_rank': ['', '.1'],
    }

    def get_trace_func(self):
        if self.is_omp:
            if self.is_graph:
                suffixes = GemForgeMicroBenchmark.OMP_GRAPH_FUNC_SUFFIX[self.benchmark_name]
                return Benchmark.ROI_FUNC_SEPARATOR.join(
                    ['.omp_outlined.' + suffix for suffix in suffixes])
            return '.omp_outlined.'
        else:
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
        # Disable the loop unswitch to test for fault_stream.
        # Default no AVX512
        flags = [
            # '-fno-unroll-loops',
            # '-fno-vectorize',
            # '-fno-slp-vectorize',
            # '-mavx512f',
            # '-ffast-math',
            # '-ffp-contract=off',
            '-mllvm',
            '-loop-unswitch-threshold=1',
        ]
        avx512_workloads = [
            'omp_array_sum',
            'omp_conv3d2',
            'omp_dense_mv_blk',
            'omp_dense_mv',
        ]
        if self.benchmark_name in avx512_workloads:
            flags.append('-mavx512f')
        no_unroll_workloads = [
            'omp_bfs',
            'omp_page_rank',
        ]
        if self.benchmark_name in no_unroll_workloads:
            flags.append('-fno-unroll-loops')
            flags.append('-fno-vectorize')

        if self.is_omp:
            flags.append('-fopenmp')
        sources = [self.source, self.graph_utils_source]
        bcs = [s[:-2] + '.bc' for s in sources]
        for source, bytecode in zip(sources, bcs):
            compile_cmd = [
                C.CC,
                source,
                '-O3',
                '-c',
                '-DGEM_FORGE',
                '-Rpass-analysis=loop-vectorize',
            ] + flags + [
                '-emit-llvm',
                '-std=c11',
                '-gline-tables-only',
                '-I{INCLUDE}'.format(INCLUDE=C.GEM5_INCLUDE_DIR),
                '-o',
                bytecode,
            ]
            Util.call_helper(compile_cmd)
        # Link into a single bc.
        Util.call_helper(['llvm-link', '-o', bc] + bcs)
        # Remove the bcs.
        Util.call_helper(['rm'] + bcs)
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

    def get_additional_gem5_simulate_command(self):
        """
        Some benchmarks takes too long to finish, so we use work item
        to ensure that we simualte for the same amount of work.
        """
        work_items = -1
        if self.benchmark_name == 'omp_page_rank':
            # One iteration, two kernels.
            work_items = 2
        if self.benchmark_name == 'omp_bfs':
            # Try to finish them?
            work_items = -1
        if work_items == -1:
            # This benchmark can finish.
            return list()
        return [
            '--work-end-exit-count={v}'.format(v=work_items)
        ]


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
