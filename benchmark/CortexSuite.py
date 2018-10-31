import Util
import Constants as C
from Benchmark import Benchmark

import os


class CortexBenchmark(Benchmark):

    FLAGS = {
        'rbm': ['O2'],
        'sphinx': ['O2'],
        'srr': ['O2'],
        'lda': ['O2'],
        'svd3': ['O2'],
        'pca': ['O2'],
        'motion-estimation': ['O2'],
        'liblinear': ['O2'],
    }

    DEFINES = {
        'rbm': {
            'small': {
                'USERS': 10,
                'TEST_USERS': 10,
                'MOVIES': 10,
                'LOOPS': 20,
            }
        },
        'sphinx': {
            'small': {}
        },
        'srr': {
            'small': {
                'SYNTHETIC1': None,
            }
        },
        'lda': {
            'small': {}
        },
        'svd3': {
            'small': {}
        },
        'pca': {
            'small': {}
        },
        'motion-estimation': {
            'small': {
                'SYNTHETIC1': None,
            }
        },
        'liblinear': {
            'small': {}
        },
    }

    INCLUDES = {
        'rbm': {
            'includes'
        },
        'sphinx': {
            'includes'
        },
        'srr': {
            'includes'
        },
        'lda': {
            'includes'
        },
        'svd3': {
            'includes'
        },
        'pca': {
            'includes'
        },
        'motion-estimation': {
            'includes'
        },
        'liblinear': {
            'includes'
        },
    }

    TRACE_FUNC = {
        'rbm': ['train'],
        'sphinx': ['fsg_search_hyp', 'ngram_search_hyp', 'phone_loop_search_hyp'],
        'srr': ['CalculateA', 'SRREngine123'],
        'lda': ['lda_mle', 'lda_inference'],
        'svd3': ['svd'],
        'pca': ['corcol'],
        'motion-estimation': ['Motion_Est'],
        'liblinear': ['train_one'],
    }

    ARGS = {
        'rbm': {
            'small': [],
        },
        'sphinx': {
            'small': ['medium/audio.raw', 'language_model/HUB4/'],
        },
        'srr': {
            'small': [],
        },
        'lda': {
            'small': ['est', '.1', '3', 'settings.txt', 'small/small_data.dat', 'random', 'small/result'],
        },
        'svd3': {
            'small': ['small.txt'],
        },
        'pca': {
            'small': ['small.data', '1593', '256', 'R'],
        },
        'motion-estimation': {
            'small': [],
        },
        'liblinear': {
            'small': ['data/100M/crime_scale'],
        },
    }

    def __init__(self, folder, benchmark_name, input_size):
        self.benchmark_name = benchmark_name
        self.input_size = input_size
        self.work_path = os.path.join(folder, self.benchmark_name)
        self.cwd = os.getcwd()
        self.flags = CortexBenchmark.FLAGS[self.benchmark_name]
        self.defines = CortexBenchmark.DEFINES[self.benchmark_name][self.input_size]
        self.includes = CortexBenchmark.INCLUDES[self.benchmark_name]
        self.trace_functions = '.'.join(
            CortexBenchmark.TRACE_FUNC[self.benchmark_name])
        self.max_inst = 1e7

        super(CortexBenchmark, self).__init__(
            name=self.get_name(),
            raw_bc=self.get_raw_bc(),
            links=['-lm'],
            args=CortexBenchmark.ARGS[self.benchmark_name][self.input_size],
            trace_func=self.trace_functions,
            lang='C',
        )

    def get_name(self):
        return 'cortex.{benchmark_name}.{input_size}'.format(
            benchmark_name=self.benchmark_name,
            input_size=self.input_size,
        )

    def get_run_path(self):
        return self.work_path

    def get_raw_bc(self):
        return '{name}.bc'.format(name=self.get_name())

    def get_n_traces(self):
        return 1

    def find_all_sources(self, folder):
        sources = list()
        for root, dirs, files in os.walk(folder):
            for f in files:
                if f.endswith('.c'):
                    # Special case for one lm3g_templates.c for sphinx,
                    # which should not be compiled.
                    if f == 'lm3g_templates.c' and self.benchmark_name == 'sphinx':
                        continue
                    sources.append(os.path.join(root, f))
        return sources

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

        sources = self.find_all_sources(self.work_path)
        bcs = [self.compile(s, self.flags, self.defines,
                            self.includes) for s in sources]

        raw_bc = self.get_raw_bc()
        link_cmd = [C.LLVM_LINK] + bcs + ['-o', raw_bc]
        self.debug('Linking to raw bitcode {raw_bc}'.format(raw_bc=raw_bc))
        Util.call_helper(link_cmd)

        os.chdir(self.cwd)

    def trace(self):
        os.chdir(self.work_path)
        self.build_trace(
            link_stdlib=False,
            trace_reachable_only=False,
            # debugs=['TracePass']
        )
        os.putenv('LLVM_TDG_MAX_INST', str(int(self.max_inst)))
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


class CortexSuite:
    FOLDER = '/home/zhengrong/Documents/CortexSuite/cortex'

    def __init__(self, folder=None, input_size='small'):
        if folder is None:
            folder = CortexSuite.FOLDER
        self.benchmarks = list()
        for benchmark_name in CortexBenchmark.INCLUDES:
            benchmark = CortexBenchmark(folder, benchmark_name, input_size)
            self.benchmarks.append(benchmark)

    def get_benchmarks(self):
        return self.benchmarks
