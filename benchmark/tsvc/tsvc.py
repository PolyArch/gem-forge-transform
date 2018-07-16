import subprocess
import os

from ..Benchmark import Benchmark
from .. import Util
from .. import Constants as C


def compile(source, vectorize, defines=[], includes=[]):
    bc_file = source.replace('.c', '.bc')
    compile_cmd = [
        'ecc',
        source,
        '-c',
        '-emit-llvm',
        '-o',
        bc_file,
        '-O3',
        '-Wall',
        '-Wno-unused-label',
        '-fno-inline-functions',
        '-fno-slp-vectorize',
        '-fno-unroll-loops',
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
    return bc_file


class TSVCBenchmark:

    # Copy from tests.h
    TESTS_DEFINE = {
        'LINEAR_DEPENDENCE': (1 << 0),
        'INDUCTION_VARIABLE': (1 << 1),
        'GLOBAL_DATA_FLOW': (1 << 2),
        'CONTROL_FLOW': (1 << 3),
        'SYMBOLICS': (1 << 4),
        'STATEMENT_REORDERING': (1 << 5),
        'LOOP_RESTRUCTURING': (1 << 6),
        'NODE_SPLITTING': (1 << 7),
        'EXPANSION': (1 << 8),
        'CROSSING_THRESHOLDS': (1 << 9),
        'REDUCTIONS': (1 << 10),
        'RECURRENCES': (1 << 11),
        'SEARCHING': (1 << 12),
        'PACKING': (1 << 13),
        'LOOP_REROLLING': (1 << 14),
        'EQUIVALENCING': (1 << 15),
        'INDIRECT_ADDRESSING': (1 << 16),
        'CONTROL_LOOPS': (1 << 17),
    }

    TESTS_FUNCTION_NAMES = {
        'LINEAR_DEPENDENCE': [
            's000',
            's111',
            's1111',
            's112',
            's1112',
            's113',
            's1113',
            's114',
            's115',
            's1115',
            's116',
            's118',
            's119',
            's1119',
        ],
        'INDUCTION_VARIABLE': [
            's121',
            's122',
            's123',
            's124',
            's125',
            's126',
            's127',
            's128',
            's453',
        ],
        'GLOBAL_DATA_FLOW': [
            's131',
            's132',
            's141',
            's151',
            's152',
            's431',
            's451',
            's452',
            's471',
            's4121',
        ],
        'CONTROL_FLOW': [
            's161',
            's1161',
            's162',
            's271',
            's272',
            's273',
            's274',
            's275',
            's2275',
            's276',
            's277',
            's278',
            's279',
            's1279',
            's2710',
            's2711',
            's2712',
            's441',
            's442',
            's443',
            's481',
            's482',
        ],
        'SYMBOLICS': [
            's171',
            's172',
            's173',
            's174',
            's175',
            's176',
        ],
        'STATEMENT_REORDERING': [
            's211',
            's212',
            's1213',
        ],
        'LOOP_RESTRUCTURING': [
            's221',
            's1221',
            's222',
            's231',
            's232',
            's1232',
            's233',
            's2233',
            's235',
        ],
        'NODE_SPLITTING': [
            's241',
            's242',
            's243',
            's244',
            's1244',
            's2244',
        ],
        'EXPANSION': [
            's251',
            's1251',
            's2251',
            's3251',
            's252',
            's253',
            's254',
            's255',
            's256',
            's257',
            's258',
            's261',
        ],
        'CROSSING_THRESHOLDS': [
            's281',
            's1281',
            's291',
            's292',
            's293',
            's2101',
            's2102',
            's2111',
        ],
        'REDUCTIONS': [
            's311',
            's31111',
            's312',
            's313',
            's314',
            's315',
            's316',
            's317',
            's318',
            's319',
            's3110',
            's13110',
            's3111',
            's3112',
            's3113',
        ],
        'RECURRENCES': [
            's321',
            's322',
            's323',
        ],
        'SEARCHING': [
            's331',
            's332',
        ],
        'PACKING': [
            's341',
            's342',
        ],
        'LOOP_REROLLING': [
            's351',
            's1351',
            's352',
            's353',
        ],
        'EQUIVALENCING': [
            's421',
            's1421',
            's422',
            's423',
            's424',
        ],
        'INDIRECT_ADDRESSING': [
            's491',
            's4112',
            's4113',
            's4114',
            's4115',
            's4116',
            's4117',
        ],
        'CONTROL_LOOPS': [
            'va',
            'vag',
            'vas',
            'vif',
            'vpv',
            'vtv',
            'vpvtv',
            'vpvts',
            'vpvpv',
            'vtvtv',
            'vsumr',
            'vdotr',
            'vbor',
        ],
    }

    TESTS_SINGLE_TEST = {
        'LINEAR_DEPENDENCE': [
            # Compiler and TDG will vectorize:
            # 's000()',
            # 's111()',
            # 's1111()',
            # 's1112()',
            # 's1115()',
            # 's1119()',
            # Compiler and TDG will not vectorize:
            # 's112()', # Iter-iter memory dependence.
            # 's114()', # Iter-iter memory dependence.
            # 's116()', # Iter-iter memory dependence.
            # 's118()', # Iter-iter memory dependence.
            # Compiler will not vectorize, but TDG can:
            # 's113()',
            # 's1113()',
            # 's115()',
            # 's119()',
        ],
        # 'INDUCTION_VARIABLE': [
        #     's121()',
        #     's122()',
        #     's123()',
        #     's124()',
        #     's125()',
        #     's126()',
        #     's127()',
        #     's128()',
        #     's453()',
        # ],
        # 'GLOBAL_DATA_FLOW': [
        #     's131()',
        #     's132()',
        #     's141()',
        #     's151()',
        #     's152()',
        #     's431()',
        #     's451()',
        #     's452()',
        #     's471()',
        #     's4121()',
        # ],
        'CONTROL_FLOW': [
            # clang and tdg will not vectorize:
            # 's161()',
            # 's1161()',
            's162(n1)',
            # clang and tdg will vectorize:
        #     's271()',
        #     's272()',
        #     's273()',
        #     's274()',
        #     's275()',
        #     's2275()',
        #     's276()',
        #     's277()',
        #     's278()',
        #     's279()',
        #     's1279()',
        #     's2710()',
        #     's2711()',
        #     's2712()',
        #     's441()',
        #     's442()',
        #     's443()',
        #     's481()',
        #     's482()',
        ],
        # 'SYMBOLICS': [
        #     's171()',
        #     's172()',
        #     's173()',
        #     's174()',
        #     's175()',
        #     's176()',
        # ],
        # 'STATEMENT_REORDERING': [
        #     's211()',
        #     's212()',
        #     's1213()',
        # ],
        # 'LOOP_RESTRUCTURING': [
        #     's221()',
        #     's1221()',
        #     's222()',
        #     's231()',
        #     's232()',
        #     's1232()',
        #     's233()',
        #     's2233()',
        #     's235()',
        # ],
        # 'NODE_SPLITTING': [
        #     's241()',
        #     's242()',
        #     's243()',
        #     's244()',
        #     's1244()',
        #     's2244()',
        # ],
        # 'EXPANSION': [
        #     's251()',
        #     's1251()',
        #     's2251()',
        #     's3251()',
        #     's252()',
        #     's253()',
        #     's254()',
        #     's255()',
        #     's256()',
        #     's257()',
        #     's258()',
        #     's261()',
        # ],
        # 'CROSSING_THRESHOLDS': [
        #     's281()',
        #     's1281()',
        #     's291()',
        #     's292()',
        #     's293()',
        #     's2101()',
        #     's2102()',
        #     's2111()',
        # ],
        # 'REDUCTIONS': [
        #     's311()',
        #     's31111()',
        #     's312()',
        #     's313()',
        #     's314()',
        #     's315()',
        #     's316()',
        #     's317()',
        #     's318()',
        #     's319()',
        #     's3110()',
        #     's13110()',
        #     's3111()',
        #     's3112()',
        #     's3113()',
        # ],
        # 'RECURRENCES': [
        #     's321()',
        #     's322()',
        #     's323()',
        # ],
        # 'SEARCHING': [
        #     's331()',
        #     's332()',
        # ],
        # 'PACKING': [
        #     's341()',
        #     's342()',
        # ],
        # 'LOOP_REROLLING': [
        #     's351()',
        #     's1351()',
        #     's352()',
        #     's353()',
        # ],
        # 'EQUIVALENCING': [
        #     's421()',
        #     's1421()',
        #     's422()',
        #     's423()',
        #     's424()',
        # ],
        # 'INDIRECT_ADDRESSING': [
        #     's491()',
        #     's4112()',
        #     's4113()',
        #     's4114()',
        #     's4115()',
        #     's4116()',
        #     's4117()',
        # ],
        # 'CONTROL_LOOPS': [
        #     'va()',
        #     'vag()',
        #     'vas()',
        #     'vif()',
        #     'vpv()',
        #     'vtv()',
        #     'vpvtv()',
        #     'vpvts()',
        #     'vpvpv()',
        #     'vtvtv()',
        #     'vsumr()',
        #     'vdotr()',
        #     'vbor()',
        # ],
    }

    NTIMES = {
        's114()': 100,
    }

    def __init__(self, test_name, test_func):
        self.test_name = test_name
        self.test_func = test_func
        self.links = ['-lm']
        # The first argument is the number of iterations.
        # The second one is the digits the the check precision.
        if self.test_func in TSVCBenchmark.NTIMES:
            self.args = [str(TSVCBenchmark.NTIMES[self.test_func])]
        else:
            self.args = ['10000']
        # We define some variables.
        # We also skip check and init to speed things up in gem5.
        self.defines = [
            'TESTS={TESTS}'.format(
                TESTS=TSVCBenchmark.TESTS_DEFINE[self.test_name]),
            'TEST_FUNC={FUNC}'.format(FUNC=test_func),
            'LLVM_TDG_SKIP_INIT',
            'LLVM_TDG_SKIP_CHECK',
        ]
        self.trace_functions = ','.join(
            TSVCBenchmark.TESTS_FUNCTION_NAMES[self.test_name])
        self.benchmark = Benchmark(
            self.get_name(), self.get_raw_bc(), self.links, self.args, self.trace_functions)

    def get_name(self):
        func_name = self.test_func.split('(')[0]
        return 'tsc.{test}.{func}'.format(test=self.test_name, func=func_name)

    def get_raw_bc(self):
        return '{name}.bc'.format(name=self.get_name())

    def get_baseline_binary(self, vectorize):
        binary = '{name}.baseline'.format(name=self.get_name())
        if vectorize:
            binary += '.simd'
        return binary

    def get_baseline_result(self, vectorize):
        return 'results/{base}.stats.txt'.format(base=self.get_baseline_binary(vectorize))

    def get_replay_result(self, vectorize):
        return 'results/{name}.replay.{simd}stats.txt'.format(
            name=self.get_name(),
            simd=('simd.' if vectorize else '')
        )

    def get_trace(self):
        return '{name}.trace'.format(name=self.get_name())

    def clean(self):
        clean_cmd = [
            'rm',
            '-f',
            self.get_raw_bc(),
            self.get_baseline_binary(True),
            self.get_baseline_binary(False),
            self.get_trace(),
        ]
        Util.call_helper(clean_cmd)
        self.benchmark.clean()

    def build_raw_bc(self):
        # By defining the TEST we control which test cases
        # we want to run.
        self.debug('Building raw bitcode...')
        tsc_bc = compile('tsc.c', False, self.defines)
        dummy_bc = compile('dummy.c', False, self.defines)
        link_cmd = [
            'llvm-link',
            tsc_bc,
            dummy_bc,
            '-o',
            self.get_raw_bc()
        ]
        Util.call_helper(link_cmd)
        # Do not forget naming everything!
        name_cmd = [
            'opt',
            '-instnamer',
            self.get_raw_bc(),
            '-o',
            self.get_raw_bc(),
        ]
        Util.call_helper(name_cmd)

    def build_baseline(self, vectorize):
        defines = list(self.defines)
        defines.append('LLVM_TDG_GEM5_BASELINE')
        includes = [C.GEM5_INCLUDE_DIR]
        tsc_bc = compile('tsc.c', vectorize, defines, includes)
        dummy_bc = compile('dummy.c', vectorize, defines, includes)
        output_binary = self.get_baseline_binary(vectorize)
        GEM5_PSEUDO_S = C.GEM5_M5OPS_X86
        build_cmd = [
            'ecc',
            '-static',
            '-o',
            output_binary,
            '-I{INCLUDE_DIR}'.format(INCLUDE_DIR=C.GEM5_INCLUDE_DIR),
            tsc_bc,
            dummy_bc,
            GEM5_PSEUDO_S,
        ]
        build_cmd += self.links
        self.debug('Building baseline {binary}...'.format(
            binary=output_binary))
        Util.call_helper(build_cmd)

    def run_baseline(self, vectorize):

        # Create the result directory.
        Util.call_helper(['mkdir', '-p', 'results'])

        GEM5_OUT_DIR = '{cpu_type}.baseline'.format(cpu_type=C.CPU_TYPE)
        Util.call_helper(['mkdir', '-p', GEM5_OUT_DIR])

        binary = self.get_baseline_binary(vectorize)
        gem5_args = [
            C.GEM5_X86,
            '--outdir={outdir}'.format(outdir=GEM5_OUT_DIR),
            C.GEM5_LLVM_TRACE_SE_CONFIG,
            '--cmd={cmd}'.format(cmd=binary),
            '--llvm-standalone=0',
            '--llvm-issue-width={ISSUE_WIDTH}'.format(
                ISSUE_WIDTH=C.ISSUE_WIDTH),
            '--llvm-store-queue-size={STORE_QUEUE_SIZE}'.format(
                STORE_QUEUE_SIZE=C.STORE_QUEUE_SIZE),
            '--caches',
            '--l2cache',
            '--cpu-type={cpu_type}'.format(cpu_type=C.CPU_TYPE),
            '--num-cpus=1',
            '--l1d_size={l1d_size}'.format(l1d_size=C.GEM5_L1D_SIZE),
            '--l1i_size={l1i_size}'.format(l1i_size=C.GEM5_L1I_SIZE),
            '--l2_size={l2_size}'.format(l2_size=C.GEM5_L2_SIZE),
            # Exit after the work_end
            '--work-end-exit-count=1',
        ]
        # Set the arguments.
        if self.args is not None:
            gem5_args.append(
                '--options={binary_args}'.format(binary_args=' '.join(self.args)))

        self.debug('Running baseline {binary} in gem5...'.format(
            binary=binary))
        Util.call_helper(gem5_args)

        # Copy out the result.
        Util.call_helper([
            'cp',
            os.path.join(GEM5_OUT_DIR, 'stats.txt'),
            self.get_baseline_result(vectorize)
        ])

    def baseline(self):
        # First do the non-vectorized version.
        self.build_baseline(False)
        self.run_baseline(False)
        # Then the vectorized version.
        self.build_baseline(True)
        self.run_baseline(True)

    def trace(self):
        self.benchmark.build_trace()
        self.benchmark.run_trace(self.get_trace())

    def build_replay(self, vectorize):
        pass_name = 'replay'
        debugs = ['ReplayPass', 'TDGSerializer']
        if vectorize:
            pass_name = 'simd-pass'
            # debugs.append('SIMDPass')
        self.benchmark.build_replay(
            pass_name=pass_name,
            trace_file=self.get_trace(),
            tdg_detail='standalone',
            debugs=debugs,
        )

    def run_replay(self, vectorize):
        output_dir = self.benchmark.gem5_replay(standalone=1)
        Util.call_helper([
            'cp',
            os.path.join(output_dir, 'stats.txt'),
            self.get_replay_result(vectorize)
        ])

    def replay(self):
        self.build_replay(False)
        self.run_replay(False)
        self.build_replay(True)
        self.run_replay(True)

    def compute_speedup(self, variable, scalar, vectorized):
        return scalar[variable] / vectorized[variable]

    def analyze(self):
        baseline_stats = Util.Gem5Stats(
            self.get_name(), self.get_baseline_result(False))
        baseline_simd_stats = Util.Gem5Stats(
            self.get_name(), self.get_baseline_result(True))
        replay_stats = Util.Gem5Stats(
            self.get_name(), self.get_replay_result(False))
        replay_simd_stats = Util.Gem5Stats(
            self.get_name(), self.get_replay_result(True))
        variables = [
            'system.cpu.numCycles',
            'system.cpu.commit.committedInsts',
            'system.cpu.commit.committedOps',
        ]
        for variable in variables:
            baseline_speedup = self.compute_speedup(
                variable,
                baseline_stats,
                baseline_simd_stats)
            replay_speedup = self.compute_speedup(
                variable,
                replay_stats,
                replay_simd_stats)
            self.debug('{variable:>35} {baseline} {replay}'.format(
                variable=variable,
                baseline=baseline_speedup,
                replay=replay_speedup))

    def debug(self, msg):
        print('> {name} <: {msg}'.format(name=self.get_name(), msg=msg))


def main():
    # First get the ground truth for compiler's choice on vectorization.
    # compile('tsc.c', True)

    # Test some group.
    tsvc_benchmarks = dict()
    # Intialize the benchmarks.
    for test_name in TSVCBenchmark.TESTS_SINGLE_TEST:
        tsvc_benchmarks[test_name] = dict()
        for test_func in TSVCBenchmark.TESTS_SINGLE_TEST[test_name]:
            benchmark = TSVCBenchmark(test_name, test_func)
            benchmark.baseline()
            benchmark.build_raw_bc()
            benchmark.trace()
            benchmark.replay()
            benchmark.clean()
            tsvc_benchmarks[test_name][test_func] = benchmark

    for test_name in TSVCBenchmark.TESTS_SINGLE_TEST:
        for test_func in TSVCBenchmark.TESTS_SINGLE_TEST[test_name]:
            benchmark = tsvc_benchmarks[test_name][test_func]
            benchmark.analyze()


if __name__ == '__main__':
    import sys
    path = '.'
    if len(sys.argv) > 1:
        path = sys.argv[1]
    current_path = os.getcwd()
    os.chdir(path)
    main()
    os.chdir(current_path)
