import Util
import Constants as C
from Benchmark import Benchmark

import os


class SDVBSBenchmark(Benchmark):
    FLAGS = {
        'disparity': ['O2'],
        'localization': ['O2'],
        # 'mser': ['O2'],
        'multi_ncut': ['O2'],
        'sift': ['O2'],
        'stitch': ['O2'],
        'svm': ['O2'],
        'texture_synthesis': ['O2'],
        'tracking': ['O2'],
    }

    DEFINES = {
        'disparity': {'GCC': None},
        'localization': {'GCC': None},
        'mser': {'GCC': None},
        'multi_ncut': {'GCC': None},
        'sift': {'GCC': None},
        'stitch': {'GCC': None},
        'svm': {'GCC': None},
        'texture_synthesis': {'GCC': None},
        'tracking': {'GCC': None},
    }

    TRACE_FUNC = {
        'disparity': [
            'computeSAD',
            'integralImage2D2D',
            'finalSAD',
            'findDisparity'
        ],
        'localization': ['workload'],
        'mser': ['mser'],
        'multi_ncut': ['segment_image'],
        'sift': ['sift', 'normalizeImage'],
        'stitch': ['getANMS', 'extractFeatures'],
        'svm': ['workload'],
        'texture_synthesis': ['create_all_candidates', 'create_candidates', 'compare_neighb', 'compare_rest', 'compare_full_neighb'],
        'tracking': [
            'imageBlur',
            'imageResize',
            'calcSobel_dX',
            'calcSobel_dY',
            'calcGoodFeature',
            'fReshape',
            'fillFeatures',
            'fTranspose',
            'getANMS',
        ],
    }

    TRACE_IDS = {
        # 'disparity': [0],
        # 'localization': [0],
        # 'mser': [0],
        # 'multi_ncut': [0],
        # 'sift': [0],
        # 'stitch': [0],
        # 'svm': [0],
        # 'texture_synthesis': [0],
        # 'tracking': [0]
        'disparity': [0, 1, 2, 3, 5, 6, 7, 8],
        'localization': [0, 4],
        'mser': [0, 1, 2, 3, 4, 5, 6, 7, 8],
        'multi_ncut': [0, 1, 2, 3, 4, 5, 6],
        'sift': [0, 1, 2, 3, 4, 5, 6, 7, 8],
        'stitch': [0, 1, 2, 3, 4, 5, 6, 7, 8],
        'svm': [0, 1, 2, 3, 4, 5, 6, 7, 8],
        'texture_synthesis': [0, 1, 2, 3, 4, 5, 6, 7, 8],
        'tracking': [0, 1, 4, 5, 6, 7, 8]
    }

    def __init__(self, folder, benchmark_name, input_name):
        self.top_folder = folder
        self.benchmark_name = benchmark_name
        self.input_name = input_name

        # Find the directories.
        self.benchmark_dir = os.path.join(
            self.top_folder, 'benchmarks', self.benchmark_name)
        self.input_dir = os.path.join(
            self.benchmark_dir, 'data', self.input_name)
        self.source_dir = os.path.join(self.benchmark_dir, 'src', 'c')
        self.common_src_dir = os.path.join(self.top_folder, 'common', 'c')

        self.cwd = os.getcwd()
        self.work_path = os.path.join(
            self.benchmark_dir, 'gem-forge-{input_name}'.format(input_name=input_name))
        self.source_bc_dir = os.path.join(self.work_path, 'obj')
        self.common_src_bc_dir = os.path.join(self.work_path, 'common_obj')
        # Create the workpath.
        Util.call_helper(['mkdir', '-p', self.work_path])
        Util.call_helper(['mkdir', '-p', self.source_bc_dir])
        Util.call_helper(['mkdir', '-p', self.common_src_bc_dir])

        self.flags = SDVBSBenchmark.FLAGS[self.benchmark_name]
        self.defines = SDVBSBenchmark.DEFINES[self.benchmark_name]
        # Also define the input name.
        self.defines[self.input_name] = None
        self.includes = [self.source_dir, self.common_src_dir]
        self.trace_functions = '.'.join(
            SDVBSBenchmark.TRACE_FUNC[self.benchmark_name])

        self.trace_ids = SDVBSBenchmark.TRACE_IDS[self.benchmark_name]
        self.start_inst = 1
        self.max_inst = 1e7
        self.skip_inst = 1e8
        self.end_inst = 10e8

        # Create the args.
        args = [self.input_dir, self.work_path]

        super(SDVBSBenchmark, self).__init__(
            name=self.get_name(),
            raw_bc=self.get_raw_bc(),
            links=['-lm'],
            args=args,
            trace_func=self.trace_functions,
            lang='C',
        )

    def get_name(self):
        return 'sdvbs.{benchmark_name}.{input_name}'.format(
            benchmark_name=self.benchmark_name,
            input_name=self.input_name,
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
                    sources.append(os.path.join(root, f))
        return sources

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

        sources = self.find_all_sources(self.source_dir)
        bcs = [self.compile(s, self.flags, self.defines,
                            self.includes, self.source_bc_dir) for s in sources]
        common_sources = self.find_all_sources(self.common_src_dir)
        bcs += [self.compile(s, self.flags, self.defines,
                             self.includes, self.common_src_bc_dir) for s in common_sources]

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
        os.putenv('LLVM_TDG_START_INST', str(int(self.start_inst)))
        os.putenv('LLVM_TDG_END_INST', str(int(self.end_inst)))
        os.putenv('LLVM_TDG_SKIP_INST', str(int(self.skip_inst)))
        os.putenv('LLVM_TDG_MEASURE_IN_TRACE_FUNC', 'TRUE')
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


class SDVBSSuite:
    # FOLDER = '/home/zhengrong/Documents/CortexSuite/vision'
    FOLDER = '/media/zhengrong/My Passport/Documents/CortexSuite/vision'

    def __init__(self, folder=None):
        if folder is None:
            self.folder = SDVBSSuite.FOLDER
        else:
            self.folder = folder
        self.benchmarks = list()
        input_names = [
            # 'sqcif',
            # 'qcif',
            # 'cif',
            # 'vga',
            'fullhd',
        ]
        for input_name in input_names:
            for benchmark_name in SDVBSBenchmark.FLAGS:
                benchmark = SDVBSBenchmark(
                    self.folder, benchmark_name, input_name)
                self.benchmarks.append(benchmark)

    def get_benchmarks(self):
        return self.benchmarks
