
from Benchmark import Benchmark
from Benchmark import BenchmarkArgs

from Utils import TraceFlagEnum

import Constants as C
import Util

import os


class ParboilBenchmark(Benchmark):

    ROI_FUNCS = {
        'histo': [
            '.omp_outlined..14',
        ],
        'mri-gridding': [
            '.omp_outlined.',
        ],
        'mri-q': [
            '.omp_outlined..1', # ComputeQCPU
            '.omp_outlined..6', # main
            '.omp_outlined..11', # ComputePhiMagCPU
        ],
        'sgemm': [
            '.omp_outlined..19',
        ],
        'spmv': [
            '.omp_outlined..7',
        ],
        'stencil': [
            '.omp_outlined..3',
        ],
        'tpacf': [
            '.omp_outlined..11',
        ],
    }

    ARGS = {
        'histo': {
            'large': ['-t', '$NTHREADS', '-i', '{DATA}/input/img.bin', '-o', '{RUN}/ref.bmp', '--', '100'],
        },
        'mri-gridding': {
            'large': ['-t', '$NTHREADS', '-i', '{DATA}/input/small.uks', '--', '32', '0'],
        },
        'mri-q': {
            'large': ['-t', '$NTHREADS', '-i', '{DATA}/input/64_64_64_dataset.bin'],
        },
        'sgemm': {
            'medium': ['-t', '$NTHREADS', '-i', '{DATA}/input/matrix1.txt,{DATA}/input/matrix2t.txt', '-o', '{RUN}/matrix3.txt'],
            'large': ['-t', '$NTHREADS', '-i', '{DATA}/input/matrix1.txt,{DATA}/input/matrix2t.txt', '-o', '{RUN}/matrix3.txt'],
        },
        'spmv': {
            'small': ['-t', '$NTHREADS', '-i', '{DATA}/input/1138_bus.mtx,{DATA}/input/vector.bin', '-o', '{RUN}/1138_bus.mtx.out'],
            'large': ['-t', '$NTHREADS', '-i', '{DATA}/input/Dubcova3.mtx.bin,{DATA}/input/vector.bin', '-o', '{RUN}/Dubcova3.mtx.out'],
        },
        'stencil': {
            'small': ['-t', '$NTHREADS', '-i', '{DATA}/input/128x128x32.bin', '-o', '{RUN}/128x128x32.out', '--', '128', '128', '32', '100'],
            'large': ['-t', '$NTHREADS', '-i', '{DATA}/input/512x512x64x100.bin', '-o', '{RUN}/512x512x64x100.out', '--', '512', '512', '64', '100'],
        },
        'tpacf': {
            'large': ['-t', '$NTHREADS', '-i', '{DATA}/input/Randompnts.{i}', '-o', '{RUN}/tpacf.out', '--', '-n', '100', '-p', '10391'],
        },
    }

    def __init__(self, benchmark_args, src_path):
        self.cwd = os.getcwd()
        self.src_path = src_path
        self.suite_path = os.path.dirname(os.path.dirname(self.src_path))
        self.benchmark_name = os.path.basename(self.src_path)
        self.n_thread = benchmark_args.options.input_threads

        self.sim_input_size = 'small'
        if benchmark_args.options.sim_input_size:
            self.sim_input_size = benchmark_args.options.sim_input_size

        # Create the result dir out of the source tree.
        self.work_path = os.path.join(
            C.LLVM_TDG_RESULT_DIR, 'parboil', self.benchmark_name
        )
        Util.mkdir_chain(self.work_path)

        self.datasets_folder = os.path.join(
            self.suite_path,
            'datasets',
            self.benchmark_name,
            self.sim_input_size,
        )
        self.run_folder = os.path.join(
            self.src_path,
            'run',
            self.sim_input_size,
        )
        Util.mkdir_chain(self.run_folder)

        super(ParboilBenchmark, self).__init__(benchmark_args)

    def get_name(self):
        return 'parboil.{b}'.format(b=self.benchmark_name)

    def get_raw_bc(self):
        # We have a special raw bc name.
        return self.benchmark_name + '.bc'

    def get_links(self):
        links = [
            '-lm',
            '-lomp',
            '-lpthread',
            '-Wl,--no-as-needed',
            '-ldl',
        ]
        return links

    def _get_args(self, input_size):
        args = list()
        for arg in ParboilBenchmark.ARGS[self.benchmark_name][input_size]:
            if arg == '$NTHREADS':
                args.append(str(self.n_thread))
            else:
                # Special case for tpacf input.
                if self.benchmark_name == 'tpacf' and input_size == 'large' and arg.find('Randompnts') != -1:
                    random_points = ','.join(
                        [arg.format(DATA=self.datasets_folder, i=i) for i in range(1, 101)])
                    data_point = os.path.join(
                        self.datasets_folder, 'input/Datapnts.1')
                    args.append(data_point + ',' + random_points)
                    continue
                args.append(arg.format(
                    DATA=self.datasets_folder, RUN=self.run_folder))
        return args

    def get_args(self):
        assert(False)
        return None

    def get_sim_args(self):
        return self._get_args(self.sim_input_size)

    def get_sim_input_size(self):
        return self.sim_input_size

    def get_trace_func(self):
        if self.benchmark_name in ParboilBenchmark.ROI_FUNCS:
            roi_funcs = ParboilBenchmark.ROI_FUNCS[self.benchmark_name]
            return Benchmark.ROI_FUNC_SEPARATOR.join(
                roi_funcs
            )
        return None

    def get_lang(self):
        return 'C'

    def get_exe_path(self):
        return self.work_path

    def get_run_path(self):
        return self.work_path

    def build_raw_bc(self):
        # First we make the edge list.
        os.chdir(self.suite_path)
        # Clean it.
        Util.call_helper([
            './parboil',
            'clean',
            self.benchmark_name,
            'omp_base',
            'gem_forge',
        ])
        # Make the bc using the special target.
        Util.call_helper([
            './parboil',
            'compile',
            self.benchmark_name,
            'omp_base',
            'gem_forge',
        ])
        # Simpy move the bc to the work path.
        Util.call_helper([
            'mv',
            os.path.join(self.src_path, 'build/omp_base_gem_forge',
                         self.get_raw_bc()),
            os.path.join(self.work_path, self.get_raw_bc()),
        ])
        os.chdir(self.cwd)

    def run_profile(self):
        # Simply set the ROI env before we profile as we only care
        # about the specified function.
        os.putenv('LLVM_TDG_TRACE_ROI', str(
            TraceFlagEnum.GemForgeTraceROI.SpecifiedFunction.value
        ))
        super(ParboilBenchmark, self).run_profile()
        os.unsetenv('LLVM_TDG_TRACE_ROI')

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

    WORK_ITEMS = {
        'histo': 1,
        'mri-gridding': 1,
        'mri-q': 3,
        'sgemm': 1,
        'spmv': 2,  # Two commands.
        'stencil': 2,
        'tpacf': 5,
    }

    def get_additional_gem5_simulate_command(self):
        if self.benchmark_name in ParboilBenchmark.WORK_ITEMS:
            return [
                '--work-end-exit-count={v}'.format(
                    v=ParboilBenchmark.WORK_ITEMS[self.benchmark_name])
            ]
        return list()


class ParboilBenchmarks:
    def __init__(self, benchmark_args):
        suite_folder = os.getenv('PARBOIL_SUITE_PATH')
        self.benchmarks = list()
        for name in [
            'histo',
            'mri-gridding',
            'mri-q',
            'spmv',
            'sgemm',
            'stencil',
            'tpacf',
        ]:
            src_path = os.path.join(suite_folder, 'benchmarks', name)
            self.benchmarks.append(
                ParboilBenchmark(benchmark_args, src_path)
            )

    def get_benchmarks(self):
        return self.benchmarks
