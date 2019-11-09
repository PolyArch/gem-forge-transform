import Util
import Constants as C
from Benchmark import Benchmark

from Utils import TraceFlagEnum

import os

"""
disparity.fullhd
26% finalSAD
24% integralImage2D2D
17% padarray4
16% computeSAD
14% findDisparity

=================================
localization.fullhd
! Multiple startTiming()
82% weightedSample

=================================
mser.fullhd
96% mser 

=================================
multi_ncut.fullhd
90% fSortIndices 
Top level is segment_image(), but I trace fSortIndices.

=================================
sift.fullhd
95% imsmooth 
    2D convolution.

=================================
stitch.fullhd
47% ffConv2 
    2D convolution.
25% getANMS
19% fDeepCopy

=================================
svm.fullhd
33% calc_learned_func
28% fMtimes
26% polynomial
10% fDeepCopyRange

=================================
texture_synthesis.fullhd
52% compare_neighb
41% compare_rest

=================================
tracking.fullhd
! Multiple startTiming()
27% imageBlur 
20% calcSobel_dY
18% calcSobel_dX
10% imageResize

"""


class SDVBSBenchmark(Benchmark):

    O2 = ['O2']
    O2_NO_VECTORIZE = ['O3', 'fno-vectorize',
                       'fno-slp-vectorize', 'fno-unroll-loops']

    # Fractal experiments.
    FLAGS_FRACTAL = {
        'disparity': O2_NO_VECTORIZE,
        'localization': O2_NO_VECTORIZE,
        'mser': O2_NO_VECTORIZE,
        'multi_ncut': O2_NO_VECTORIZE,
        'sift': O2_NO_VECTORIZE,
        'stitch': O2_NO_VECTORIZE,
        'svm': O2_NO_VECTORIZE,
        'texture_synthesis': O2_NO_VECTORIZE,
        'tracking': O2_NO_VECTORIZE,
    }

    # Stream experiments.
    FLAGS_STREAM = {
        'disparity': O2_NO_VECTORIZE,
        'localization': O2_NO_VECTORIZE,
        'mser': O2_NO_VECTORIZE,
        'multi_ncut': O2_NO_VECTORIZE,
        'sift': O2_NO_VECTORIZE,
        'stitch': O2_NO_VECTORIZE,
        'svm': O2_NO_VECTORIZE,
        'texture_synthesis': O2_NO_VECTORIZE,
        'tracking': O2_NO_VECTORIZE,
    }

    DEFINES = {
        'GCC': None,
        'GEM_FORGE': None,
    }

    TRACE_FUNC = {
        'disparity': [
            'computeSAD',
            'integralImage2D2D',
            'finalSAD',
            'findDisparity'
        ],
        'localization': ['gem_forge_work'],
        'mser': ['mser'],
        'multi_ncut': ['fSortIndices'],
        'sift': ['sift', 'normalizeImage'],
        'stitch': ['getANMS', 'harris', 'extractFeatures'],
        'svm': ['gem_forge_work'],
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
        # 'localization': [4],
        # 'mser': [0],
        # 'multi_ncut': [6],
        # 'sift': [1],
        # 'stitch': [5],
        # 'svm': [0],
        # 'texture_synthesis': [0],
        # 'tracking': [8],
        'disparity': [0, 1, 2, 3, 5, 6, 7, 8, 9],
        'localization': [0, 4],
        'mser': [0, 1, 2, 5, 7, 8, 9],
        'multi_ncut': [0, 1, 2, 3, 4, 5, 6, 9],
        'sift': [0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
        'stitch': [0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
        'svm': [0, 1, 2, 3, 4, 5, 6, 7, 8],
        'texture_synthesis': [0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
        'tracking': [0, 1, 5, 6, 7, 8, 9]
    }

    LEGAL_INPUT_SIZE = ('test', 'sim_fast', 'sim',
                        'sqcif', 'qcif', 'cif', 'vga', 'fullhd')

    def __init__(self,
                 benchmark_args,
                 folder, benchmark_name,
                 suite='sdvbs'):
        self.top_folder = folder
        self.benchmark_name = benchmark_name
        self.input_size = 'fullhd'
        if benchmark_args.options.input_size:
            self.input_size = benchmark_args.options.input_size
        assert(self.input_size in SDVBSBenchmark.LEGAL_INPUT_SIZE)
        self.sim_input_size = 'fullhd'
        if benchmark_args.options.sim_input_size:
            self.sim_input_size = benchmark_args.options.sim_input_size
        assert(self.sim_input_size in SDVBSBenchmark.LEGAL_INPUT_SIZE)

        self.suite = suite

        # Find the directories.
        self.benchmark_dir = os.path.join(
            self.top_folder, 'benchmarks', self.benchmark_name)
        self.source_dir = os.path.join(self.benchmark_dir, 'src', 'c')
        self.common_src_dir = os.path.join(self.top_folder, 'common', 'c')

        self.cwd = os.getcwd()

        # Create the result directory outside of the source tree.
        self.work_path = os.path.join(
            C.LLVM_TDG_RESULT_DIR, self.suite, self.benchmark_name)
        Util.mkdir_chain(self.work_path)

        self.source_bc_dir = os.path.join(self.work_path, 'obj')
        self.common_src_bc_dir = os.path.join(self.work_path, 'common_obj')
        Util.mkdir_chain(self.source_bc_dir)
        Util.mkdir_chain(self.common_src_bc_dir)

        if C.EXPERIMENTS == 'stream':
            self.flags = SDVBSBenchmark.FLAGS_STREAM[self.benchmark_name]
        elif C.EXPERIMENTS == 'fractal':
            self.flags = SDVBSBenchmark.FLAGS_FRACTAL[self.benchmark_name]
        self.flags.append('gline-tables-only')
        self.defines = SDVBSBenchmark.DEFINES
        self.includes = [
            self.source_dir,
            self.common_src_dir,
            C.GEM5_INCLUDE_DIR,
        ]
        self.trace_functions = '|'.join(
            SDVBSBenchmark.TRACE_FUNC[self.benchmark_name])

        self.trace_ids = SDVBSBenchmark.TRACE_IDS[self.benchmark_name]
        self.start_inst = 1
        self.max_inst = 1e7
        self.skip_inst = 1e8
        self.end_inst = 10e8

        super(SDVBSBenchmark, self).__init__(
            benchmark_args
        )

    def get_name(self):
        return '{suite}.{benchmark_name}'.format(
            suite=self.suite,
            benchmark_name=self.benchmark_name
        )

    def get_input_size(self):
        return self.input_size

    def get_sim_input_size(self):
        return self.sim_input_size

    def get_links(self):
        return ['-lm']

    def get_args(self):
        input_dir = os.path.join(
            self.benchmark_dir, 'data', self.input_size)
        return [input_dir]

    def get_sim_args(self):
        input_dir = os.path.join(
            self.benchmark_dir, 'data', self.sim_input_size)
        return [input_dir]

    def get_trace_func(self):
        return self.trace_functions

    def get_lang(self):
        return 'C'

    def get_exe_path(self):
        return self.work_path

    def get_run_path(self):
        return self.work_path

    def get_perf_frequency(self):
        # multi_ncut runs for too long.
        if 'multi_ncut' in self.get_name():
            return 1
        else:
            return 63500

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
        os.chdir(self.get_exe_path())
        self.build_trace(
            link_stdlib=False,
            trace_reachable_only=False,
        )

        if self.input_size != 'fullhd':
            # For non fullhd input we trace only the traced function.
            os.putenv('LLVM_TDG_TRACE_ROI', str(
                TraceFlagEnum.GemForgeTraceROI.SpecifiedFunction.value
            ))
            os.putenv('LLVM_TDG_TRACE_MODE', str(
                TraceFlagEnum.GemForgeTraceMode.TraceAll.value
            ))
            # We trace at most 1 million instructions.
            os.putenv('LLVM_TDG_HARD_EXIT_IN_MILLION', str(100))
        else:
            # Otherwise we trace the simpoint region.
            os.putenv('LLVM_TDG_TRACE_ROI', str(
                TraceFlagEnum.GemForgeTraceROI.All.value
            ))
            os.putenv('LLVM_TDG_TRACE_MODE', str(
                TraceFlagEnum.GemForgeTraceMode.TraceSpecifiedInterval.value
            ))
            os.putenv('LLVM_TDG_INTERVALS_FILE', self.get_simpoint_abs())

        self.run_trace()
        os.chdir(self.cwd)


class SDVBSSuite:

    def __init__(self, benchmark_args):
        self.folder = os.getenv('SDVBS_SUITE_PATH')
        self.benchmarks = list()
        for benchmark_name in SDVBSBenchmark.FLAGS_FRACTAL:
            benchmark = SDVBSBenchmark(
                benchmark_args,
                self.folder, benchmark_name)
            self.benchmarks.append(benchmark)

    def get_benchmarks(self):
        return self.benchmarks
