import os

from Benchmark import Benchmark
import Util
import Constants as C


def compile(source, vectorize, output_bc, defines=[], includes=[], link_stdlib=False):
    compiler = C.CC if source.endswith('.c') else C.CXX
    compile_cmd = [
        compiler,
        source,
        '-c',
        '-emit-llvm',
        '-o',
        output_bc,
        '-O3',
        '-Wall',
        '-Wno-unused-label',
        '-fno-slp-vectorize',
        '-fno-unroll-loops',
        '-finline-functions',
    ]
    if vectorize:
        compile_cmd += [
            '-Rpass=loop-vectorize',
            '-Rpass-analysis=loop-vectorize',
            '-fsave-optimization-record',
            # For our experiment we enforce the vectorize width.
            '-DLLVM_TDG_VEC_WIDTH=4',
        ]
    else:
        compile_cmd += [
            '-fno-vectorize',
        ]
    for define in defines:
        compile_cmd.append('-D{DEFINE}'.format(DEFINE=define))
    for include in includes:
        compile_cmd.append('-I{INCLUDE}'.format(INCLUDE=include))
    Util.call_helper(compile_cmd)
    if link_stdlib:
        link_cmd = [
            C.LLVM_LINK,
            output_bc,
            C.MUSL_LIBC_LLVM_BC,
            C.LLVM_COMPILER_RT_BC,
            '-o',
            output_bc,
        ]
        Util.call_helper(link_cmd)
    Util.call_helper([C.OPT, '-instnamer', output_bc, '-o', output_bc])
    Util.call_helper([C.LLVM_DIS, output_bc])


class SPUBenchmark(Benchmark):

    TESTS = [
        'ac',
        'fc-layer',
        'ksvm',
        'scnn',
        'gbdt',
    ]

    LANG = {
        'ac': 'C',
        'fc-layer': 'C',
        'ksvm': 'C',
        'scnn': 'C',
        'gbdt': 'CPP',
    }

    TRACE_FUNCS = {
        'ac': [
            'forwardPropagation',
            'backPropagation',
        ],
        'fc-layer': [
            'mv_mult',
        ],
        'ksvm': [
            'train',
        ],
        'scnn': [
            'convolution_layer_blocked',
        ],
        'gbdt': {
            'DecisionTree::build_tree(TNode*)',
        },
    }

    TRACE_STDLIB = {
        'ac': False,
        'fc-layer': False,
        'ksvm': False,
        'scnn': False,
        'gbdt': False,
    }

    def __init__(self, folder, test_name):
        self.test_name = test_name
        self.cwd = os.getcwd()
        self.work_path = os.path.join(self.cwd, folder, self.test_name)

        self.defines = []
        self.trace_functions = '.'.join(
            SPUBenchmark.TRACE_FUNCS[self.test_name])
        self.trace_stdlib = SPUBenchmark.TRACE_STDLIB[self.test_name]
        super(SPUBenchmark, self).__init__(
            name=self.get_name(),
            raw_bc=self.get_raw_bc(),
            links=['-lm'],
            args=None,
            trace_func=self.trace_functions,
            lang=SPUBenchmark.LANG[self.test_name],
        )

    def get_name(self):
        if self.trace_stdlib:
            return 'spu.{test}.std'.format(test=self.test_name)
        else:
            return 'spu.{test}'.format(test=self.test_name)

    def get_run_path(self):
        return self.work_path

    def get_trace_ids(self):
        return [0]

    def build_raw_bc(self):
        # By defining the TEST we control which test cases
        # we want to run.
        self.debug('Building raw bitcode...')
        os.chdir(self.work_path)
        source = 'cpu.cpp' if self.test_name == 'gbdt' else 'cpu.c'
        compile(
            source=source,
            vectorize=False,
            output_bc=self.get_raw_bc(),
            defines=self.defines,
            link_stdlib=self.trace_stdlib)
        Util.call_helper(['make', 'input.data'])
        os.chdir(self.cwd)

    def trace(self):
        os.chdir(self.work_path)
        # If we trace the stdlib, make sure that we only
        # trace the necessary ones.
        self.build_trace(
            link_stdlib=self.trace_stdlib,
            trace_reachable_only=self.trace_stdlib,
            # debugs=['TracePass']
        )
        self.run_trace()
        os.chdir(self.cwd)


class SPUBenchmarks:
    def __init__(self):
        self.benchmarks = list()
        for test_name in SPUBenchmark.TESTS:
            benchmark = SPUBenchmark(folder, test_name)
            self.benchmarks.append(benchmark)

    def get_benchmarks(self):
        return self.benchmarks
