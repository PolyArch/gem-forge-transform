import os
import sys
import subprocess

import Constants as C
import Util
from Benchmark import Benchmark


class SPEC2017Benchmark(Benchmark):

    def __init__(self, name, target, max_insts, raw_bc, links, args, trace_func, skip_inst):
        super(SPEC2017Benchmark, self).__init__(
            name=target,
            raw_bc=raw_bc,
            links=links,
            args=args,
            trace_func=trace_func,
            skip_inst=skip_inst
        )
        self.name = name
        self.target = target
        self.max_insts = max_insts



class SPEC2017Benchmarks:

    BENCHMARK_PARAMS = {
        # 'lbm_s': {
        #     'name': '619.lbm_s',
        #     'links': ['-lm'],
        #     'skip_inst': 0,
        #     'max_insts': 100000000,
        #     'trace_func': 'LBM_performStreamCollideTRT'
        # },
        # 'imagick_s': {
        #     'name': '638.imagick_s',
        #     'links': ['-lm'],
        #     'skip_inst': 0,
        #     'max_insts': 100000000,
        #     'trace_func': 'MagickCommandGenesis',
        # },
        # 'nab_s': {
        #     'name': '644.nab_s',
        #     'links': ['-lm'],
        #     'skip_inst': 0,
        #     'max_insts': 100000000,
        #     'trace_func': 'md',
        # },
        # 'x264_s': {
        #     'name': '625.x264_s',
        #     'links': [],
        #     'skip_inst': 100000000,
        #     'max_insts': 100000000,
        #     'trace_func': 'x264_encoder_encode',
        # },
        # 'mcf_s': {
        #     'name': '605.mcf_s',
        #     'links': [],
        #     'skip_inst': 0,
        #     'max_insts': 100000000,
        #     'trace_func': 'global_opt',
        # },
        'xz_s': {
            'name': '657.xz_s',
            'links': [],
            'skip_inst': 0,
            'max_insts': 100000000,
            'trace_func': 'spec_compress,spec_uncompress',
        },
        # 'gcc_s': {
        #     'name': '602.gcc_s',
        #     'links': [],
        #     'skip_inst': 100000000,
        #     'max_insts': 100000000,
        #     'trace_func': '',
        # },
        # # C++ Benchmark: Notice that SPEC is compiled without
        # # debug information, and we have to use mangled name
        # # for trace_func.
        # 'deepsjeng_s': {
        #     'name': '631.deepsjeng_s',
        #     'links': [],
        #     'skip_inst': 100000000,
        #     'max_insts': 100000000,
        #     'trace_func': '',
        # },
        # 'leela_s': {
        #     'name': '641.leela_s',
        #     'links': [],
        #     'skip_inst': 0,
        #     'max_insts': 100000000,
        #     'trace_func': '_ZN9UCTSearch5thinkEii', # Search::think
        # },
        # # perlbench_s is not working as it uses POSIX open function.
        # 'perlbench_s': {
        #     'name': '600.perlbench_s',
        #     'links': [],
        #     'skip_inst': 100000000,
        #     'max_insts': 100000000,
        #     'trace_func': '',
        # },

    }

    CFLAGS = [
        '-O3',
        '-Wall',
        '-Wno-unused-label',
        '-fno-inline-functions',
        '-fno-vectorize',
        '-fno-slp-vectorize'
    ]

    def __init__(self, trace_stdlib, force=False):
        self.spec = os.environ.get('SPEC')
        if self.spec is None:
            print(
                'Unable to find SPEC env var. Please source shrc before using this script')
            sys.exit()

        # Remember to setup gllvm.
        self.set_gllvm()

        self.cwd = os.getcwd()

        self.trace_stdlib = trace_stdlib
        self.force = force

        Util.call_helper(['mkdir', '-p', 'result'])

        self.benchmarks = dict()
        for target in SPEC2017Benchmarks.BENCHMARK_PARAMS:
            name = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['name']
            max_insts = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['max_insts']
            skip_inst = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['skip_inst']
            trace_func = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['trace_func']
            links = SPEC2017Benchmarks.BENCHMARK_PARAMS[target]['links']

            # If we want to trace the std library, then we have to change the links.
            if self.trace_stdlib:
                links = [
                    '-static',
                    '-nostdlib',
                    C.MUSL_LIBC_CRT,
                    C.MUSL_LIBC_STATIC_LIB,
                    C.LLVM_COMPILER_RT_BUILTIN_LIB,
                    C.LLVM_UNWIND_STATIC_LIB,
                    # '-lgcc_s',
                    # '-lgcc',
                    # C++ run time library for tracer.
                    '-lstdc++',
                    # '-lc++',
                    # '-lc++abi',
                ]

            args = self.initBenchmark(name, target)
            self.benchmarks[target] = SPEC2017Benchmark(
                name=name,
                target=target,
                max_insts=max_insts,
                raw_bc=self.getRawBitcodeName(target),
                links=links,
                args=args,
                trace_func=trace_func,
                skip_inst=skip_inst
            )

    def getRunPath(self, name):
        return os.path.join(
            self.spec,
            'benchspec',
            'CPU',
            name,
            'run',
            'run_base_refspeed_LLVM_TDG-m64.0000'
        )

    def getBuildPath(self, name):
        return os.path.join(
            self.spec,
            'benchspec',
            'CPU',
            name,
            'build',
            'build_base_LLVM_TDG-m64.0000'
        )

    def getRawBitcodeName(self, target):
        return target + '.bc'

    def getArgs(self, specinvoke):
        for line in specinvoke.split('\n'):
            if len(line) == 0:
                continue
            if line[0] == '#':
                continue
            # Found one.
            fields = line.split()
            assert(len(fields) > 6)
            # Ignore the first one and redirect one.
            # Also ignore other invoke for now.
            return fields[1:-5]

    def set_gllvm(self):
        # Set up the environment for gllvm.
        os.putenv('LLVM_CC_NAME', 'ecc')
        os.putenv('LLVM_CXX_NAME', 'ecc++')
        os.putenv('LLVM_LINK_NAME', 'llvm-link')
        os.putenv('LLVM_AR_NAME', 'ecc-ar')

    def baseline(self, benchmark, subbenchmark):
        path = os.path.join(self.cwd, self.folder, benchmark, subbenchmark)
        os.chdir(path)
        self.buildBaseline(benchmark, subbenchmark)
        gem5_outdir = self.gem5Baseline(benchmark, subbenchmark)
        # Copy the result out.
        os.chdir(self.cwd)
        Util.call_helper(['cp', os.path.join(gem5_outdir, 'stats.txt'), os.path.join(
            self.cwd, 'result', self.getName(benchmark, subbenchmark) + '.baseline.txt')])

    def trace(self, target):
        benchmark = self.benchmarks[target]
        os.chdir(self.getRunPath(benchmark.name))
        benchmark.build_trace()
        # set the maximum number of insts.
        if benchmark.max_insts != -1:
            os.putenv('LLVM_TDG_MAXIMUM_INST', str(benchmark.max_insts))
        else:
            os.unsetenv('LLVM_TDG_MAXIMUM_INST')
        benchmark.run_trace()
        os.chdir(self.cwd)

    def build_replay(self, target):
        benchmark = self.benchmarks[target]
        os.chdir(self.getRunPath(benchmark.name))
        benchmark.build_replay()
        os.chdir(self.cwd)

    # Spec benchmark is generally too complicated to be handled by
    # our normal address base calculation. So we will only run it in
    # standalone mode.
    def run_standalone(self, target):
        benchmark = self.benchmarks[target]
        os.chdir(self.getRunPath(benchmark.name))
        gem5_outdir = benchmark.gem5_replay(standalone=1)
        # Copy the result out.
        os.chdir(self.cwd)
        Util.call_helper(['cp', os.path.join(gem5_outdir, 'stats.txt'), os.path.join(
            self.cwd, 'result', target + '.standalone.txt')])

    def buildRawBitcode(self, name, target):
        # Fake the runcpu.
        fake_cmd = [
            'runcpu',
            '--fake',
            '--config=ecc-llvm-linux-x86_64',
            '--tune=base',
            name
        ]
        Util.call_helper(fake_cmd)
        # Actually build it.
        os.chdir(self.getBuildPath(name))
        try:
            # First try build without target.
            Util.call_helper(['specmake'])
        except subprocess.CalledProcessError as e:
            # Something goes wrong if there are multiple targets,
            # try build with target explicitly specified.
            build_cmd = [
                'specmake',
                'TARGET={target}'.format(target=target)
            ]
            Util.call_helper(build_cmd)

        # If we are going to trace the stdlib.
        raw_bc = self.getRawBitcodeName(target)
        # Extract the bitcode from the binary.
        binary = target
        # Special rule for gcc_s, whose binary is sgcc
        if target == 'gcc_s':
            binary = 'sgcc'
        extract_cmd = [
            'get-bc',
            '-o',
            raw_bc,
            binary
        ]
        print('# Extracting bitcode from binary...')
        Util.call_helper(extract_cmd)
        if self.trace_stdlib:
            # Link with the llvm bitcode of standard library.
            link_cmd = [
                'llvm-link',
                raw_bc,
                C.MUSL_LIBC_LLVM_BC,
                '-o',
                raw_bc
            ]
            print('# Link with stdlib...')
            Util.call_helper(link_cmd)

        # Name everything in the bitcode.
        print('# Naming everything in the llvm bitcode...')
        Util.call_helper(['opt', '-instnamer', raw_bc, '-o', raw_bc])
        # Copy it to run directory.
        Util.call_helper([
            'cp',
            raw_bc,
            self.getRunPath(name)
        ])
        os.chdir(self.cwd)

    """
    Set up the benchmark.

    Parameters
    ----------
    benchmark: the name of the benchmark.

    """

    def initBenchmark(self, name, target):
        if self.force:
            # Clear the build/run path.
            clear_cmd = [
                'rm',
                '-rf',
                self.getBuildPath(name)
            ]
            Util.call_helper(clear_cmd)
            clear_cmd = [
                'rm',
                '-rf',
                self.getRunPath(name)
            ]
            Util.call_helper(clear_cmd)
        # Check if build dir and run dir exists.
        if not os.path.isdir(self.getBuildPath(name)):
            self.buildRawBitcode(name, target)
        if not os.path.isdir(self.getRunPath(name)):
            self.buildRawBitcode(name, target)
        # Finally use specinvoke to get the arguments.
        os.chdir(self.getRunPath(name))
        args = self.getArgs(subprocess.check_output([
            'specinvoke',
            '-n'
        ]))
        print('{name} has arguments {args}'.format(name=name, args=args))
        os.chdir(self.cwd)
        return args


def main(folder):
    trace_stdlib = False
    force = False
    benchmarks = SPEC2017Benchmarks(trace_stdlib, force)
    names = list()
    for target in SPEC2017Benchmarks.BENCHMARK_PARAMS:
        benchmarks.trace(target)
        # benchmarks.build_replay(target)
        # benchmarks.run_standalone(target)
        names.append(target)
    #benchmarks.draw('MachSuite.baseline.replay.pdf', 'baseline', 'replay', names)
    #benchmarks.draw('MachSuite.standalone.replay.pdf', 'standalone', 'replay', names)


if __name__ == '__main__':
    import sys
    if len(sys.argv) == 1:
        # Use the current folder.
        main('.')
    else:
        main(sys.argv[1])
