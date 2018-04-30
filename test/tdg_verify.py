
import matplotlib.pyplot as plt
import numpy
import os
import subprocess


GEM5_PATH = '/home/sean/Public/gem5'
GEM5_X86 = GEM5_PATH + '/build/X86/gem5.opt'
GEM5_L1D_SIZE = '32kB'
GEM5_L1I_SIZE = '32kB'
GEM5_L2_SIZE = '256kB'
GEM5_L3_SIZE = '6144kB'
GEM5_STATS_FN = 'stats.txt'

RESULT_DIR = 'results-i8-raw'
ISSUE_WIDTH = 8
STORE_QUEUE_SIZE = 2


def make_target(target, workload_size):
    subprocess.check_call(
        ['make', 'CFLAGS=-DTDG_WORKLOAD_SIZE={workload_size}'.format(workload_size=workload_size), target])


def run_gem5(binary, binary_args, debug_flags=''):
    CPU_TYPE = 'DerivO3CPU'
    GEM5_OUT_DIR = CPU_TYPE
    GEM5_SE_CONFIG = GEM5_PATH + '/configs/example/llvm_trace_replay.py'
    subprocess.check_call(['mkdir', '-p', GEM5_OUT_DIR])
    gem5_args = [
        GEM5_X86,
        '--outdir={outdir}'.format(outdir=GEM5_OUT_DIR),
        '--debug-flags={debug_flags}'.format(debug_flags=debug_flags),
        GEM5_SE_CONFIG,
        '--cmd={cmd}'.format(cmd=binary),
        '--llvm-issue-width={ISSUE_WIDTH}'.format(ISSUE_WIDTH=ISSUE_WIDTH),
        '--llvm-store-queue-size={STORE_QUEUE_SIZE}'.format(STORE_QUEUE_SIZE=STORE_QUEUE_SIZE),
        '--options={binary_args}'.format(binary_args=binary_args),
        '--caches',
        '--l2cache',
        '--cpu-type={cpu_type}'.format(cpu_type=CPU_TYPE),
        '--num-cpus=1',
        '--l1d_size={l1d_size}'.format(l1d_size=GEM5_L1D_SIZE),
        '--l1i_size={l1i_size}'.format(l1i_size=GEM5_L1I_SIZE),
        '--l2_size={l2_size}'.format(l2_size=GEM5_L2_SIZE),
    ]
    if debug_flags == '':
        gem5_args.remove(gem5_args[2])
    debug_output_file = 'gem5.debug.{flags}.txt'.format(flags=debug_flags)
    with open(debug_output_file, 'w') as debug_output:
        subprocess.check_call(gem5_args, stdout=debug_output)
    return GEM5_OUT_DIR


def run_gem5_llvm_trace_cpu(binary, binary_args):
    CPU_TYPE = 'DerivO3CPU'
    GEM5_OUT_DIR = '{cpu_type}.replay'.format(cpu_type=CPU_TYPE)
    GEM5_SE_CONFIG = os.path.join(
        GEM5_PATH, 'configs/example/llvm_trace_replay.py')
    LLVM_TRACE_FN = 'llvm_trace_gem5.txt'
    subprocess.check_call(['mkdir', '-p', GEM5_OUT_DIR])
    gem5_args = [
        GEM5_X86,
        '--outdir={outdir}'.format(outdir=GEM5_OUT_DIR),
        # '--debug-flags=LLVMTraceCPU',
        GEM5_SE_CONFIG,
        '--cmd={cmd}'.format(cmd=binary),
        '--llvm-trace-file={trace_file}'.format(trace_file=LLVM_TRACE_FN),
        '--llvm-issue-width={ISSUE_WIDTH}'.format(ISSUE_WIDTH=ISSUE_WIDTH),
        '--llvm-store-queue-size={STORE_QUEUE_SIZE}'.format(STORE_QUEUE_SIZE=STORE_QUEUE_SIZE),
        '--options={binary_args}'.format(binary_args=binary_args),
        '--caches',
        '--l2cache',
        '--cpu-type={cpu_type}'.format(cpu_type=CPU_TYPE),
        '--num-cpus=1',
        '--l1d_size={l1d_size}'.format(l1d_size=GEM5_L1D_SIZE),
        '--l1i_size={l1i_size}'.format(l1i_size=GEM5_L1I_SIZE),
        '--l2_size={l2_size}'.format(l2_size=GEM5_L2_SIZE),
        # '--l3_size={l3_size}'.format(l3_size=GEM5_L3_SIZE),
    ]
    subprocess.check_call(gem5_args)
    return GEM5_OUT_DIR


def load_scalar_variables(gem5_stats_fn):
    results = dict()
    with open(gem5_stats_fn, 'r') as gem5_stats:
        for line in gem5_stats:
            if len(line) == 0:
                continue
            if line[0] == '-':
                continue
            fields = line.split()
            try:
                results[fields[0]] = float(fields[1])
            except Exception as e:
                print('ignore line {line}'.format(line=line))
    return results


def compare(res1, res2, var):
    assert var in res1
    assert var in res2
    diff = res1[var] - res2[var]
    print("Diff {var} {res1} {res2} {diff} {percentage}%".format(
        var=var, res1=res1[var], res2=res2[var], diff=diff, percentage=diff / res1[var] * 100))


compared_vars = ['sim_ticks', 'sim_insts', 'sim_ops']


def get_stats_fn(binary, workload_size, suffix):
    stats = './{RESULT_DIR}/{binary}_{workload}_{suffix}.txt'.format(
        RESULT_DIR=RESULT_DIR,
        binary=binary,
        workload=workload_size,
        suffix=suffix)
    stats = os.path.join(os.getcwd(), stats)
    return stats


def run_single_test(binary, binary_args, workload_size):
    # Make the results.
    subprocess.check_call(['mkdir', '-p', RESULT_DIR])
    # Clean all the built binaries.
    make_target('clean', 0)
    make_target('clean_trace', 0)
    make_target('clean_replay', 0)
    # Build normal binary.
    make_target(binary, workload_size)
    # Run it in gem5.
    gem5_out_dir_normal = run_gem5(binary, binary_args)

    # Copy out the stats.
    stats_normal = get_stats_fn(binary, workload_size, 'normal')
    subprocess.check_call([
        'mv',
        os.path.join(gem5_out_dir_normal, 'stats.txt'),
        stats_normal,
    ])

    # Build the trace binary.
    binary_trace = binary + '_trace'
    make_target(binary_trace, workload_size)
    # Run it for the trace.
    trace_args = binary_args.split()
    trace_args.insert(0, './{binary}'.format(binary=binary_trace))
    subprocess.check_call(trace_args)
    # Build the replay binary.
    binary_replay = binary + '_replay'
    make_target(binary_replay, workload_size)
    # Run it in gem5.
    gem5_out_dir_replay = run_gem5_llvm_trace_cpu(binary_replay, binary_args)
    # Copy out the stats.
    stats_replay = get_stats_fn(binary, workload_size, 'replay')
    subprocess.check_call([
        'mv',
        os.path.join(gem5_out_dir_replay, 'stats.txt'),
        stats_replay,
    ])
    return (workload_size, stats_normal, stats_replay)


def fit_stats(normals, replays):
    fit = numpy.polyfit(normals, replays, 1)
    print(
        'fit {a}*normal + {b}'.format(a=fit[0], b=fit[1]))
    # fit_fn = numpy.poly1d(fit)
    # plt.plot(normals, replays, 'yo',
    #          normals, fit_fn(normals), '--k')
    # plt.show()
    return fit


def extract_one_value(result, varialbe):
    try:
        return result[varialbe]
    except KeyError:
        return 0


def verify_one_varialbe(results, variable):
    normals = list()
    replays = list()
    for result in results:
        normal = extract_one_value(result['normal'], variable)
        replay = extract_one_value(result['replay'], variable)
        print('{workload_size} {normal} {replay}'.format(
            workload_size=result['size'], normal=normal, replay=replay))
        normals.append(normal)
        replays.append(replay)
    return fit_stats(normals, replays)


def verify_split_varialbe(results, variables):
    normals = list()
    replays = list()
    variable_normal = variables[0]
    variable_replay_cpu0 = variables[1]
    variable_replay_cpu1 = variables[2]
    for result in results:
        normal = extract_one_value(result['normal'], variable_normal)
        replay_cpu0 = extract_one_value(
            result['replay'], variable_replay_cpu0)
        replay_cpu1 = extract_one_value(
            result['replay'], variable_replay_cpu1)
        replay = replay_cpu0 + replay_cpu1
        print('{workload_size} {normal} {replay}'.format(
            workload_size=result['size'], normal=normal, replay=replay))
        normals.append(normal)
        replays.append(replay)
    return fit_stats(normals, replays)


def verify_one_binary(binary, binary_args, variables, run=True):
    # Do the test with variable workload size.
    stats = list()
    for workload_size in xrange(1, 4):
        if run:
            run_single_test(binary, binary_args, workload_size)
        # Load the stats
        stat = {
            'size': workload_size,
            'normal': load_scalar_variables(get_stats_fn(binary, workload_size, 'normal')),
            'replay': load_scalar_variables(get_stats_fn(binary, workload_size, 'replay')),
        }
        stats.append(stat)

    result = dict()

    for variable in variables:
        fieldNames = variables[variable]
        if len(fieldNames) > 1:
            fit = verify_split_varialbe(stats, fieldNames)
        else:
            fit = verify_one_varialbe(stats, fieldNames[0])
        result[variable] = fit
    return result


def verify_machsuite():

    benchmarks = {
        # 'SORT-MERGE': [
        #     './MachSuite/sort/merge',
        #     'sort',
        # ],
        # 'SORT-RADIX': [
        #     './MachSuite/sort/radix',
        #     'sort',
        # ],
        # 'SPMV-ELLPACK': [
        #     './MachSuite/spmv/ellpack',
        #     'spmv',
        # ],
        # 'FFT-STRIDE': [
        #     './MachSuite/fft/strided',
        #     'fft',
        # ],
        # 'FFT-TRANSPOSE': [
        #     './MachSuite/fft/transpose',
        #     'fft',
        # ],
        # 'KMP': [
        #     './MachSuite/kmp/kmp',
        #     'kmp',
        # ],
        # 'GEMM-BLOCKED': [
        #     './MachSuite/gemm/blocked',
        #     'gemm',
        # ],
        # 'GEMM-NCUBED': [
        #     './MachSuite/gemm/ncubed',
        #     'gemm',
        # ],
        # 'BFS-QUEUE': [
        #     './MachSuite/bfs/queue',
        #     'bfs',
        # ],
        # 'MD-GRID': [
        #     './MachSuite/md/grid',
        #     'md',
        # ],
        # 'MD-KNN': [
        #     './MachSuite/md/knn',
        #     'md',
        # ],
        # 'NW': [
        #     './MachSuite/nw/nw',
        #     'nw',
        # ],
        # 'SPMV-CRS': [
        #     './MachSuite/spmv/crs',
        #     'spmv',
        # ],
        # 'VITERBI': [
        #     './MachSuite/viterbi/viterbi',
        #     'viterbi',
        # ],
        # 'STENCIL-2D': [
        #     './MachSuite/stencil/stencil2d',
        #     'stencil',
        # ],
        # 'STENCIL-3D': [
        #     './MachSuite/stencil/stencil3d',
        #     'stencil',
        # ],
        # Execution
        # 'ED1': [
        #     './Vertical/ED1',
        #     'bench',
        # ],
        # 'EF': [
        #     './Vertical/EF',
        #     'bench',
        # ],
        # 'EI': [
        #     './Vertical/EI',
        #     'bench',
        # ],
        # 'EM1': [
        #     './Vertical/EM1',
        #     'bench',
        # ],
        # 'EM5': [
        #     './Vertical/EM5',
        #     'bench',
        # ],
        # Store Cache
        # 'STc': [
        #     './Vertical/STc',
        #     'bench',
        # ],
        # 'STL2': [
        #     './Vertical/STL2',
        #     'bench',
        # ],
        # 'STL2b': [
        #     './Vertical/STL2b',
        #     'bench',
        # ],
        # Control.
        # 'CCa': [
        #     './Vertical/CCa',
        #     'bench'
        # ],
        # 'CCm': [
        #     './Vertical/CCm',
        #     'bench'
        # ],
        # 'CS1': [
        #     './Vertical/CS1',
        #     'bench'
        # ],
        # 'CS3': [
        #     './Vertical/CS3',
        #     'bench'
        # ],
        # 'CRd': [
        #     './Vertical/CRd',
        #     'bench'
        # ],
        # 'CRf': [
        #     './Vertical/CRf',
        #     'bench'
        # ],
        # 'CCh': [
        #     './Vertical/CCh',
        #     'bench'
        # ],
        # Memory:
        # 'MM': [
        #     './Vertical/MM',
        #     'bench',
        # ],
        # 'MM_st': [
        #     './Vertical/MM_st',
        #     'bench',
        # ],
        'ML2': [
            './Vertical/ML2',
            'bench',
        ],
        # 'MI': [
        #     './Vertical/MI',
        #     'bench',
        # ],
        # 'MD': [
        #     './Vertical/MD',
        #     'bench',
        # ],
        # Data parallel.
        # 'DP1D': [
        #     './Vertical/DP1d',
        #     'bench',
        # ],
        # 'M-DYN': [
        #     './Vertical/M_Dyn',
        #     'bench',
        # ],

    }

    variables = {
        'Tick': ['sim_ticks'],
        'Write': [
            'system.cpu.dcache.WriteReq_accesses::total',
            'system.cpu0.dcache.WriteReq_accesses::total',
            'system.cpu1.dcache.WriteReq_accesses::total',
        ],
        'Read': [
            'system.cpu.dcache.ReadReq_accesses::total',
            'system.cpu0.dcache.ReadReq_accesses::total',
            'system.cpu1.dcache.ReadReq_accesses::total',
        ],
        # 'Inst': [
        #     'system.cpu.commit.committedInsts',
        #     'system.cpu0.commit.committedInsts',
        #     'system.cpu1.commit.committedInsts',
        # ],
        'MicroOp': [
            'system.cpu.commit.committedOps',
            'system.cpu0.commit.committedOps',
            'system.cpu1.commit.committedOps',
        ],
    }

    binary_args = ''
    current_path = os.getcwd()
    result = dict()

    run_list = [
        # 'GEMM-BLOCKED',
        # 'GEMM-NCUBED',
        'KMP',
    ]

    for benchmark in benchmarks:
        benchmark_dir = benchmarks[benchmark][0]
        binary = benchmarks[benchmark][1]
        os.chdir(benchmark_dir)
        run = True
        # run = False
        # if benchmark in ['VITERBI', 'FFT-STRIDE']:
            # run = False
        try:
            result[benchmark] = verify_one_binary(
                binary, binary_args, variables, run)
        except Exception:
            pass
        os.chdir(current_path)

    benchmark_names = result.keys()
    index = numpy.arange(len(benchmark_names))
    bar_width = 0.8 / len(variables)
    pos = 0.0

    print result

    for variable in variables:
        ys = [result[b][variable][0] for b in benchmark_names]
        plt.bar(index + pos, ys, bar_width,
                color=numpy.random.rand(3, 1), label=variable)
        pos += bar_width
    plt.xticks(index + 0.4, benchmark_names)
    plt.legend()
    plt.tight_layout()
    plt.show()


def main():
    # cd to path.
    verify_machsuite()


if __name__ == '__main__':
    # main('./hello', 'hello', '')
    # main('/home/sean/Documents/LLVM-TDG/test/Vertical/ED1', 'bench', '')
    # main('/home/sean/Documents/LLVM-TDG/test/Vertical/EI', 'bench', '')
    # main('/home/sean/Documents/LLVM-TDG/test/Vertical/EF', 'bench', '')
    # main('/home/sean/Documents/LLVM-TDG/test/Vertical/EM1', 'bench', '')
    # main('/home/sean/Documents/LLVM-TDG/test/Vertical/EM5', 'bench', '')
    # main('/home/sean/Documents/LLVM-TDG/test/Vertical/STc', 'bench', '')
    main()
    # main('./MachSuite/fft/transpose', 'fft', 'input.data output.data')
    # main('./MachSuite/kmp/kmp', 'kmp', 'input.data output.data')
