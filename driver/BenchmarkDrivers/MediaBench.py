import Util
import Constants as C
from Benchmark import Benchmark

from Utils import TraceFlagEnum

import os


class MediaBenchmarkMacro(object):
    def __init__(self, macros):
        self.__dict__.update(macros)


class MediaBenchmark(Benchmark):

    BENCHMARK_MACROS = {
        'cjpeg': {
            'src_folder': 'jpeg/jpeg-6a',
            'input_folder': 'jpeg/data',
            'input_data': ['testimg.ppm'],
            'srcs': [
                'jcapimin.c',
                'jcapistd.c',
                'jctrans.c',
                'jcparam.c',
                'jdatadst.c',
                'jcinit.c',
                'jcmaster.c',
                'jcmarker.c',
                'jcmainct.c',
                'jcprepct.c',
                'jccoefct.c',
                'jccolor.c',
                'jcsample.c',
                'jchuff.c',
                'jcphuff.c',
                'jcdctmgr.c',
                'jfdctfst.c',
                'jfdctflt.c',
                'jfdctint.c',
                'jdapimin.c',
                'jdapistd.c',
                'jdtrans.c',
                'jdatasrc.c',
                'jdmaster.c',
                'jdinput.c',
                'jdmarker.c',
                'jdhuff.c',
                'jdphuff.c',
                'jdmainct.c',
                'jdcoefct.c',
                'jdpostct.c',
                'jddctmgr.c',
                'jidctfst.c',
                'jidctflt.c',
                'jidctint.c',
                'jidctred.c',
                'jdsample.c',
                'jdcolor.c',
                'jquant1.c',
                'jquant2.c',
                'jdmerge.c',
                'jcomapi.c',
                'jutils.c',
                'jerror.c',
                'jmemmgr.c',
                'jmemnobs.c',
                # CJPEG specific sources.
                'cjpeg.c',
                'rdppm.c',
                'rdgif.c',
                'rdtarga.c',
                'rdrle.c',
                'rdbmp.c',
                'rdswitch.c',
                'cdjpeg.c',
            ],
            'flags': [
                'O3',
            ],
            'trace_func': 'gem_forge_work',
            'args': [
                '-dct',
                'int',
                '-progressive',
                '-opt',
                '-outfile',
                '/tmp/cjpeg-testout.jpeg',
            ]
        },
        'mpeg2enc': {
            'src_folder': 'mpeg2/src/mpeg2enc',
            'input_folder': 'mpeg2/data',
            'input_data': ['options.par', 'out.m2v'],
            'srcs': [
                'motion.c',
                'mpeg2enc.c',
                'predict.c',
                'puthdr.c',
                'putmpg.c',
                'putpic.c',
                'putseq.c',
                'putvlc.c',
                'quantize.c',
                'ratectl.c',
                'readpic.c',
                'transfrm.c',
                'writepic.c',
                'conform.c',
                'idct.c',
                'putbits.c',
                'fdctref.c',
                'stats.c',
            ],
            'flags': [
                'O3',
            ],
            'trace_func': 'putseq',
            'args': []
        },
        'mpeg2dec': {
            'src_folder': 'mpeg2/src/mpeg2dec',
            'input_folder': 'mpeg2/data',
            'input_data': ['options.par', 'out.m2v'],
            'srcs': [
                'display.c',
                'getbits.c',
                'getblk.c',
                'gethdr.c',
                'getpic.c',
                'getvlc.c',
                'idct.c',
                'idctref.c',
                'motion.c',
                'mpeg2dec.c',
                'recon.c',
                'spatscal.c',
                'store.c',
                'subspic.c',
                'systems.c',
            ],
            'flags': [
                'O3',
                'fno-vectorize',
                'fno-slp-vectorize',
            ],
            'trace_func': 'gem_forge_work',
            'args': [
                '-r',
                '-f',
                '-o0',
            ]
        }


    }

    def __init__(self,
                 benchmark_args,
                 folder, benchmark_name):
        self.top_folder = folder
        self.benchmark_name = benchmark_name

        self.macro = MediaBenchmarkMacro(
            MediaBenchmark.BENCHMARK_MACROS[self.benchmark_name])

        # Find the directories.
        self.source_dir = os.path.join(self.top_folder, self.macro.src_folder)
        self.input_dir = os.path.join(
            self.top_folder, self.macro.input_folder)

        self.cwd = os.getcwd()

        # Create the result directory out side of the source tree.
        self.work_path = os.path.join(
            C.LLVM_TDG_RESULT_DIR, 'mb', self.benchmark_name)
        Util.mkdir_chain(self.work_path)

        self.source_bc_dir = os.path.join(self.work_path, 'obj')
        Util.mkdir_chain(self.source_bc_dir)

        self.macro.flags.append('gline-tables-only')
        self.defines = dict()
        self.includes = [self.source_dir]
        # self.trace_functions = '.'.join(
        #     SDVBSBenchmark.TRACE_FUNC[self.benchmark_name])

        # self.trace_ids = SDVBSBenchmark.TRACE_IDS[self.benchmark_name]
        self.start_inst = 1
        self.max_inst = 1e7
        self.skip_inst = 1e8
        self.end_inst = 10e8

        # Create the args.
        self.args = self.macro.args
        # Add the input
        self.args += [os.path.join(self.input_dir, i)
                      for i in self.macro.input_data]

        super(MediaBenchmark, self).__init__(
            benchmark_args
        )

    def get_name(self):
        return 'mb.{benchmark_name}'.format(
            benchmark_name=self.benchmark_name,
        )

    def get_links(self):
        # Link the empty implementation of the gem5 pseudo instructions.
        gem5_pseudo = os.path.join(self.top_folder, 'gem5_pseudo.cpp')
        return [gem5_pseudo, '-lm']

    def get_args(self):
        if self.get_name() == 'mb.mpeg2dec':
            return [
                '-b',
                '{data}/mei16v2.m2v'.format(data=self.input_dir),
                '-r',
                '-f',
                '-o0',
                '{data}/tmp\%d'.format(data=self.input_dir),
            ]
        return self.args

    def get_trace_func(self):
        return self.macro.trace_func

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
            return 100

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

        sources = [os.path.join(self.source_dir, s) for s in self.macro.srcs]
        bcs = [self.compile(s, self.macro.flags, self.defines,
                            self.includes, self.source_bc_dir) for s in sources]

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

        # Trace the whole function
        os.putenv('LLVM_TDG_TRACE_MODE', str(
            TraceFlagEnum.GemForgeTraceMode.TraceAll.value))
        os.putenv('LLVM_TDG_TRACE_ROI', str(TraceFlagEnum.GemForgeTraceROI.SpecifiedFunction.value))

        self.run_trace()
        os.chdir(self.cwd)

    def get_additional_gem5_simulate_command(self):
        # For validation, we disable cache warm.
        args = ['--gem-forge-cold-cache']
        if self.get_name() == 'mb.mpeg2enc':
            # This benchmark requires significantly large memory.
            args.append('--mem-size=2GB')
        return args

    def build_validation(self, transform_config, trace, output_tdg):
        # Build the validation binary.
        print "Validation"
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


class MediaBenchSuite:

    def __init__(self, benchmark_args):
        self.folder = os.getenv('MEDIABENCH_SUITE_PATH')
        self.benchmarks = list()
        for benchmark_name in MediaBenchmark.BENCHMARK_MACROS:
            benchmark = MediaBenchmark(
                benchmark_args,
                self.folder, benchmark_name)
            self.benchmarks.append(benchmark)

    def get_benchmarks(self):
        return self.benchmarks
