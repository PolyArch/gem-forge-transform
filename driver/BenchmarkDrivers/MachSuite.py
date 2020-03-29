import os
import subprocess
import multiprocessing

from Utils import TraceFlagEnum

import Constants as C
import Util
from Benchmark import Benchmark


class MachSuiteBenchmark(Benchmark):

    COMMON_SOURCES = [
        'local_support.c',
        '../../common/support.c',
        '../../common/harness.c',
    ]

    COMMON_SOURCE_DIR = '../../common'

    CFLAGS = [
        # Be careful, we do not support integrated mode with private global variable,
        # as there is no symbol for these variables and we can not find them during
        # replay.
        'O3',
        'Wall',
        'Wno-unused-label',
        'fno-inline-functions',
        'gline-tables-only',
    ]

    def __init__(self,
                 benchmark_args, folder, benchmark, subbenchmark):
        self.benchmark_name = benchmark
        self.subbenchmark_name = subbenchmark
        self.cwd = os.getcwd()
        self.folder = folder
        self.source_dir = os.path.join(
            self.cwd,
            self.folder,
            self.benchmark_name,
            self.subbenchmark_name)
        self.gem5_pseudo_source = os.path.join(
            self.source_dir, MachSuiteBenchmark.COMMON_SOURCE_DIR, 'gem5_pseudo.cpp')
        # Create the result directory outside of the source tree.
        self.work_path = os.path.join(
            C.LLVM_TDG_RESULT_DIR, 'mach', self.benchmark_name, self.subbenchmark_name
        )
        Util.mkdir_chain(self.work_path)
        self.source_bc_dir = os.path.join(self.work_path, 'obj')
        Util.mkdir_chain(self.source_bc_dir)

        self.includes = [self.source_dir, os.path.join(
            self.source_dir, MachSuiteBenchmark.COMMON_SOURCE_DIR)]

        super(MachSuiteBenchmark, self).__init__(
            benchmark_args)

    def get_name(self):
        return 'mach.{b}.{sub}'.format(
            b=self.benchmark_name, sub=self.subbenchmark_name)

    def get_links(self):
        return ['-lm', self.gem5_pseudo_source]

    def get_args(self):
        return None

    def get_trace_func(self):
        return 'run_benchmark'

    def get_lang(self):
        return 'C'

    def get_exe_path(self):
        return self.work_path

    def get_run_path(self):
        return self.work_path

    def compile(self, source, flags, defines, includes, output_dir=None):
        compiler = C.CC if source.endswith('.c') else C.CXX
        if output_dir is not None:
            _, source_fn = os.path.split(source)
            bc = os.path.join(output_dir, source_fn + '.bc')
        else:
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
        # Hack for fSortIndexes.
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
        os.chdir(self.work_path)
        # # Generate the input. Backprop doesnot require generating the input.
        # if self.benchmark_name != 'backprop':
        #     Util.call_helper(['make', 'generate'])
        #     Util.call_helper(['./generate'])

        sources = [os.path.join(self.source_dir, self.benchmark_name + '.c')]
        for common_source in MachSuiteBenchmark.COMMON_SOURCES:
            sources.append(os.path.join(self.source_dir, common_source))
        defines = dict()
        bcs = [self.compile(s, MachSuiteBenchmark.CFLAGS,
                            defines, self.includes, self.source_bc_dir) for s in sources]
        raw_bc = self.get_raw_bc()
        link_cmd = [C.LLVM_LINK] + bcs + ['-o', raw_bc]
        Util.call_helper(link_cmd)

        os.chdir(self.cwd)

    def prepare_input(self, output_dir):
        # Generate the input.
        os.chdir(self.source_dir)
        Util.call_helper(['make', 'generate'])
        Util.call_helper(['./generate'])
        Util.call_helper(
            ['mv', 'input.data', os.path.join(output_dir, 'input.data')])
        Util.call_helper(['make', 'clean'])
        os.chdir(self.cwd)

    def trace(self):
        # Prepare the input before we change pwd.
        print("shit")
        self.prepare_input(self.get_exe_path())

        os.chdir(self.get_exe_path())
        self.build_trace(
            link_stdlib=False,
            trace_reachable_only=False,
        )

        # For this benchmark, we only trace the target function.
        os.putenv('LLVM_TDG_TRACE_MODE', str(TraceFlagEnum.GemForgeTraceMode.TraceAll.value))
        os.putenv('LLVM_TDG_TRACE_ROI', str(TraceFlagEnum.GemForgeTraceROI.SpecifiedFunction.value))
        self.run_trace()
        os.chdir(self.cwd)

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
        os.chdir(self.work_path)
        Util.call_helper(gem5_args)
        os.chdir(self.cwd)


class MachSuiteBenchmarks:

    BENCHMARK_PARAMS = {
        "bfs": [
            "queue",
            #     # "bulk" # not working
        ],
        "aes": [
            "aes"
        ],
        "stencil": [
            "stencil2d",
            "stencil3d"
        ],
        "md": [
            "grid",
            "knn"
        ],
        "fft": [
            "strided",
            "transpose"
        ],
        "viterbi": [
            "viterbi"
        ],
        "sort": [
            # "radix",
            "merge"
        ],
        "spmv": [
            "ellpack",
            "crs"
        ],
        "kmp": [
            "kmp"
        ],
        #  "backprop": [
        #     # "backprop" # Not working.
        #  ],
        "gemm": [
            "blocked",
            "ncubed"
        ],
        "nw": [
            "nw"
        ]
    }

    def __init__(self, benchmark_args):
        suite_folder = os.path.join(C.LLVM_TDG_BENCHMARK_DIR, 'MachSuite')
        self.benchmarks = list()
        for benchmark in MachSuiteBenchmarks.BENCHMARK_PARAMS:
            for subbenchmark in MachSuiteBenchmarks.BENCHMARK_PARAMS[benchmark]:
                self.benchmarks.append(MachSuiteBenchmark(
                    benchmark_args, suite_folder, benchmark, subbenchmark))

    def get_benchmarks(self):
        return self.benchmarks
