import Util
import Constants as C
from Benchmark import Benchmark

import os


class CortexBenchmark(Benchmark):

    FLAGS = {
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
        'rbm': [0, 1, 2, 3, 4, 5, 6, 7],
        'sphinx': [0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
        'srr': [0, 1, 2, 3, 4, 5, 6, 7],
        'lda': [1, 2, 3, 4],
        'svd3': [0, 1, 2, 3, 4, 5, 6, 7, 8],
        'pca': [0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
        'motion-estimation': [0, 1, 2, 3, 4, 5, 6, 7],
        'liblinear': [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10],
    }

    def __init__(self, folder, benchmark_name, input_size):
        self.benchmark_name = benchmark_name
        self.input_size = input_size
        self.top_folder = folder

        self.src_dir = os.path.join(self.top_folder, benchmark_name)

        self.top_result_dir = os.path.join(
            self.top_folder, 'gem-forge-results'
        )
        Util.call_helper(['mkdir', '-p', self.top_result_dir])
        self.benchmark_result_dir = os.path.join(
            self.top_result_dir, self.benchmark_name)
        Util.call_helper(['mkdir', '-p', self.benchmark_result_dir])
        self.work_path = os.path.join(
            self.benchmark_result_dir, input_size)
        Util.call_helper(['mkdir', '-p', self.work_path])

        # Create a symbolic link for everything in the source dir.
        for f in os.listdir(self.src_dir):
            print(os.path.join(self.src_dir, f))
            dest = os.path.join(self.work_path, f)
            if os.path.exists(dest):
                continue
            Util.call_helper([
                'ln',
                '-s',
                os.path.join(self.src_dir, f),
                dest,
            ])

        self.cwd = os.getcwd()
        self.flags = CortexBenchmark.FLAGS[self.benchmark_name]
        self.defines = CortexBenchmark.DEFINES[self.benchmark_name][self.input_size]
        self.includes = CortexBenchmark.INCLUDES[self.benchmark_name]
        self.trace_functions = '.'.join(
            CortexBenchmark.TRACE_FUNC[self.benchmark_name])

        self.trace_ids = CortexBenchmark.TRACE_IDS[self.benchmark_name]
        self.start_inst = 1
        self.max_inst = 1e7
        self.skip_inst = 1e8
        self.end_inst = 11e8

        args = list()
        for x in CortexBenchmark.ARGS[self.benchmark_name][self.input_size]:
            xx = os.path.join(self.src_dir, x)
            if os.path.isfile(xx):
                args.append(xx)
            else:
                args.append(x)

        super(CortexBenchmark, self).__init__(
            name=self.get_name(),
            raw_bc=self.get_raw_bc(),
            links=['-lm'],
            args=args,
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

    def get_trace_ids(self):
        return self.trace_ids

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
        # os.putenv('LLVM_TDG_MAX_INST', str(int(self.max_inst)))
        # os.putenv('LLVM_TDG_START_INST', str(int(self.start_inst)))
        # os.putenv('LLVM_TDG_END_INST', str(int(self.end_inst)))
        # os.putenv('LLVM_TDG_SKIP_INST', str(int(self.skip_inst)))
        # os.putenv('LLVM_TDG_MEASURE_IN_TRACE_FUNC', 'TRUE')
        os.putenv('LLVM_TDG_WORK_MODE', str(4))
        os.putenv('LLVM_TDG_INTERVALS_FILE', 'simpoints.txt')
        os.unsetenv('LLVM_TDG_MEASURE_IN_TRACE_FUNC')
        self.run_trace(self.get_name())
        os.chdir(self.cwd)

    def transform(self, transform_config, trace, profile_file, tdg, debugs):
        os.chdir(self.work_path)

        self.build_replay(
            transform_config=transform_config,
            trace_file=trace,
            profile_file=profile_file,
            # tdg_detail='integrated',
            tdg_detail='standalone',
            output_tdg=tdg,
            debugs=debugs,
        )

        os.chdir(self.cwd)


class CortexSuite:
    # FOLDER = '/home/zhengrong/Documents/CortexSuite/cortex'
    FOLDER = '/media/zhengrong/My Passport/Documents/CortexSuite/cortex'

    def __init__(self, folder=None):
        folder = os.getenv('CORTEX_SUITE_PATH')
        assert(folder is not None)
        self.benchmarks = list()
        input_sizes = [
            # 'small',
            # 'medium',
            'large',
        ]
        for input_size in input_sizes:
            for benchmark_name in CortexBenchmark.INCLUDES:
                benchmark = CortexBenchmark(folder, benchmark_name, input_size)
                self.benchmarks.append(benchmark)

    def get_benchmarks(self):
        return self.benchmarks
