import Util
import Constants as C
from Benchmark import Benchmark

from Utils import TraceFlagEnum

import os


class CortexBenchmark(Benchmark):

    FLAGS_STREAM = {
        # Advanced stream experiments.
        # RBM has vectorized loop with extra iterations.
        # So far we can not handle it.
        'rbm': ['O2', 'fno-vectorize', 'fno-slp-vectorize', 'fno-unroll-loops'],
        'sphinx': ['O2'],
        'srr': ['O2'],
        'lda': ['O2'],
        'svd3': ['O2'],
        'pca': ['O2'],
        'motion-estimation': ['O2'],
        'liblinear': ['O2'],
    }

    FLAGS_FRACTAL = {
        # Fractal experiments.
        'rbm': ['O2', 'fno-vectorize', 'fno-slp-vectorize', 'fno-unroll-loops'],
        'sphinx': ['O2', 'fno-vectorize', 'fno-slp-vectorize', 'fno-unroll-loops'],
        'srr': ['O2', 'fno-vectorize', 'fno-slp-vectorize', 'fno-unroll-loops'],
        'lda': ['O2', 'fno-vectorize', 'fno-slp-vectorize', 'fno-unroll-loops'],
        'svd3': ['O2', 'fno-vectorize', 'fno-slp-vectorize', 'fno-unroll-loops'],
        'pca': ['O2', 'fno-vectorize', 'fno-slp-vectorize', 'fno-unroll-loops'],
        'motion-estimation': ['O2', 'fno-vectorize', 'fno-slp-vectorize', 'fno-unroll-loops'],
        'liblinear': ['O2', 'fno-vectorize', 'fno-slp-vectorize', 'fno-unroll-loops'],
    }

    DEFINES = {
        'rbm': {
            'small': {
                'USERS': 10,
                'TEST_USERS': 10,
                'MOVIES': 10,
                'LOOPS': 20,
            },
            'medium': {
                'USERS': 100,
                'TEST_USERS': 100,
                'MOVIES': 100,
                'LOOPS': 20,
            },
            'large': {
                'USERS': 100,
                'TEST_USERS': 100,
                'MOVIES': 100,
                'LOOPS': 200,
            },
        },
        'sphinx': {
            'small': {},
            'medium': {},
            'large': {},
        },
        'srr': {
            'small': {
                'SYNTHETIC1': None,
            },
            'medium': {
                'ALPACA': None,
            },
            'large': {
                'BOOKCASE': None,
            },
        },
        'lda': {
            'small': {},
            'medium': {},
            'large': {},
        },
        'svd3': {
            'small': {},
            'medium': {},
            'large': {},
        },
        'pca': {
            'small': {},
            'medium': {},
            'large': {},
        },
        'motion-estimation': {
            'small': {
                'SYNTHETIC1': None,
            },
            'medium': {
                'ALPACA': None,
            },
            'large': {
                'BOOKCASE': None,
            },
        },
        'liblinear': {
            'small': {},
            'medium': {},
            'large': {},
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
            'medium': [],
            'large': [],
        },
        'sphinx': {
            'small': ['small/audio.raw', 'language_model/turtle/'],
            'medium': ['medium/audio.raw', 'language_model/HUB4/'],
            'large': ['large/audio.raw', 'language_model/HUB4/'],
        },
        'srr': {
            'small': [],
            'medium': [],
            'large': [],
        },
        'lda': {
            'small': ['est', '.1', '3', 'settings.txt', 'small/small_data.dat', 'random', 'small/result'],
            'medium': ['est', '.1', '3', 'settings.txt', 'medium/medium_data.dat', 'random', 'medium/result'],
            'large': ['est', '.1', '3', 'settings.txt', 'large/large_data.dat', 'random', 'large/result'],
        },
        'svd3': {
            'small': ['small.txt'],
            'medium': ['med.txt'],
            'large': ['large.txt'],
        },
        'pca': {
            'small': ['small.data', '1593', '256', 'R'],
            'medium': ['medium.data', '722', '800', 'R'],
            'large': ['large.data', '5000', '1059', 'R'],
        },
        'motion-estimation': {
            'small': [],
            'medium': [],
            'large': [],
        },
        'liblinear': {
            'small': ['data/100M/crime_scale'],
            'medium': ['data/10B/epsilon'],
            'large': ['data/100B/kdda'],
        },
    }

    TRACE_IDS = {
        # 'rbm': 0],
        # 'sphinx': [8],
        # 'srr': [1],
        # 'lda': [1],
        # 'svd3': [0,1,2,4,5,6,7,8,9],
        # 'pca': [1],
        # 'motion-estimation': [0,1,3,8],
        # 'liblinear': [0],
        'rbm': [0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
        'sphinx': [0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
        'srr': [0, 1, 2, 3, 4, 5, 6, 8, 9],
        'lda': [0],
        'svd3': [0, 1, 2, 4, 5, 6, 7, 8, 9],
        'pca': [0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
        'motion-estimation': [0, 1, 3, 8],
        'liblinear': [0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
    }

    def __init__(self, benchmark_args,
                 folder, benchmark_name, input_size, suite='cortex'):
        self.benchmark_name = benchmark_name
        self.input_size = input_size
        self.suite = suite
        self.top_folder = folder

        self.src_dir = os.path.join(self.top_folder, benchmark_name)

        self.work_path = os.path.join(
            C.LLVM_TDG_RESULT_DIR, self.suite, self.benchmark_name, input_size)
        Util.mkdir_chain(self.work_path)

        # Create a symbolic link for everything in the source dir.
        for f in os.listdir(self.src_dir):
            print(os.path.join(self.src_dir, f))
            source = os.path.join(self.src_dir, f)
            dest = os.path.join(self.work_path, f)
            Util.create_symbolic_link(source, dest)

        self.cwd = os.getcwd()
        if C.EXPERIMENTS == 'stream':
            self.flags = CortexBenchmark.FLAGS_STREAM[self.benchmark_name]
        elif C.EXPERIMENTS == 'fractal':
            self.flags = CortexBenchmark.FLAGS_FRACTAL[self.benchmark_name]
        self.flags.append('gline-tables-only')
        self.defines = CortexBenchmark.DEFINES[self.benchmark_name][self.input_size]
        self.includes = CortexBenchmark.INCLUDES[self.benchmark_name]
        self.trace_functions = '.'.join(
            CortexBenchmark.TRACE_FUNC[self.benchmark_name])

        self.trace_ids = CortexBenchmark.TRACE_IDS[self.benchmark_name]
        self.start_inst = 1
        self.max_inst = 1e7
        self.skip_inst = 1e8
        self.end_inst = 11e8

        self.args = CortexBenchmark.ARGS[self.benchmark_name][self.input_size]

        super(CortexBenchmark, self).__init__(benchmark_args)

    def get_name(self):
        return '{suite}.{benchmark_name}.{input_size}'.format(
            suite=self.suite,
            benchmark_name=self.benchmark_name,
            input_size=self.input_size,
        )

    def get_links(self):
        return ['-lm']

    def get_args(self):
        return self.args

    def get_trace_func(self):
        return self.trace_functions

    def get_lang(self):
        return 'C'

    def get_exe_path(self):
        return self.work_path

    def get_run_path(self):
        return self.work_path

    def get_raw_bc(self):
        return '{name}.bc'.format(name=self.get_name())

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

        sources = self.find_all_sources(self.src_dir)
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

        os.putenv('LLVM_TDG_TRACE_MODE', str(
            TraceFlagEnum.GemForgeTraceMode.TraceSpecifiedInterval.value
        ))
        os.putenv('LLVM_TDG_INTERVALS_FILE', 'simpoints.txt')
        os.unsetenv('LLVM_TDG_TRACE_ROI')

        self.run_trace(self.get_name())
        os.chdir(self.cwd)


class CortexSuite:
    # FOLDER = '/home/zhengrong/Documents/CortexSuite/cortex'
    FOLDER = '/media/zhengrong/My Passport/Documents/CortexSuite/cortex'

    def __init__(self, benchmark_args):
        folder = os.getenv('CORTEX_SUITE_PATH')
        assert(folder is not None)
        self.benchmarks = list()
        input_sizes = [
            # 'small',
            # 'medium',
            'large',
        ]
        for input_size in input_sizes:
            for benchmark_name in CortexBenchmark.FLAGS_STREAM:
                benchmark = CortexBenchmark(
                    benchmark_args, folder, benchmark_name, input_size)
                self.benchmarks.append(benchmark)

    def get_benchmarks(self):
        return self.benchmarks
