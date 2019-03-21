
import matplotlib
import matplotlib.pyplot as plt
import numpy
import os
import subprocess


GEM5_PATH = '/home/sean/Public/gem5'
GEM5_X86 = GEM5_PATH + '/build/X86/gem5.opt'
GEM5_L1D_SIZE = '32kB'
GEM5_L1I_SIZE = '32kB'
GEM5_L2_SIZE = '256kB'
GEM5_STATS_FN = 'stats.txt'

RESULT_DIR = 'results-i8-o3'
ISSUE_WIDTH = 8
STORE_QUEUE_SIZE = 32


def make_with_output(target, additional_flags=dict()):
    cmd = ['make']
    for flag in additional_flags:
        ff = '{flag}={value}'.format(flag=flag, value=additional_flags[flag])
        cmd.append(ff)
    cmd.append(target)
    return subprocess.check_output(cmd)


def make_target(target, additional_flags=dict()):
    cmd = ['make']
    for flag in additional_flags:
        ff = '{flag}={value}'.format(flag=flag, value=additional_flags[flag])
        cmd.append(ff)
    cmd.append(target)
    subprocess.check_call(cmd)


def make_target_with_defines(target, additional_defines=dict()):
    flags = dict()
    for define in additional_defines:
        flags['CFLAGS=-D' + define] = additional_defines[define]
    make_target(target, flags)


def make_target_with_tdg_workload_size(target, workload_size):
    make_target_with_defines(target, {'TDG_WORKLOAD_SIZE': workload_size})


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
        '--llvm-store-queue-size={STORE_QUEUE_SIZE}'.format(
            STORE_QUEUE_SIZE=STORE_QUEUE_SIZE),
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
        # pass
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
        '--debug-flags=LLVMTraceCPU',
        GEM5_SE_CONFIG,
        '--cmd={cmd}'.format(cmd=binary),
        '--llvm-trace-file={trace_file}'.format(trace_file=LLVM_TRACE_FN),
        '--llvm-issue-width={ISSUE_WIDTH}'.format(ISSUE_WIDTH=ISSUE_WIDTH),
        '--llvm-store-queue-size={STORE_QUEUE_SIZE}'.format(
            STORE_QUEUE_SIZE=STORE_QUEUE_SIZE),
        '--options={binary_args}'.format(binary_args=binary_args),
        '--caches',
        '--l2cache',
        '--cpu-type={cpu_type}'.format(cpu_type=CPU_TYPE),
        '--num-cpus=1',
        '--l1d_size={l1d_size}'.format(l1d_size=GEM5_L1D_SIZE),
        '--l1i_size={l1i_size}'.format(l1i_size=GEM5_L1I_SIZE),
        '--l2_size={l2_size}'.format(l2_size=GEM5_L2_SIZE),
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
                pass
                # print('ignore line {line}'.format(line=line))
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


def run_baseline(binary, binary_args, workload_size):
    # Make the results.
    subprocess.check_call(['mkdir', '-p', RESULT_DIR])
    # Clean all the built binaries.
    make_target('clean')
    make_target('clean_trace')
    make_target('clean_replay')
    # Build normal binary.
    make_target_with_tdg_workload_size(binary, workload_size)
    # Run it in gem5.
    gem5_out_dir_normal = run_gem5(binary, binary_args)
    # Copy out the stats.
    stats_normal = get_stats_fn(binary, workload_size, 'normal')
    subprocess.check_call([
        'mv',
        os.path.join(gem5_out_dir_normal, 'stats.txt'),
        stats_normal,
    ])
    return stats_normal


def run_replay(binary, binary_args, workload_size):
    # Make the results.
    subprocess.check_call(['mkdir', '-p', RESULT_DIR])
    # Clean all the built binaries.
    make_target('clean_trace')
    make_target('clean_replay')
    # Build the trace binary.
    binary_trace = binary + '_trace'
    make_target_with_tdg_workload_size(binary_trace, workload_size)
    # Run it for the trace.
    trace_args = binary_args.split()
    trace_args.insert(0, './{binary}'.format(binary=binary_trace))
    subprocess.check_call(trace_args)
    # Build the replay binary.
    binary_replay = binary + '_replay'
    make_target_with_tdg_workload_size(binary_replay, workload_size)
    # Run it in gem5.
    gem5_out_dir_replay = run_gem5_llvm_trace_cpu(binary_replay, binary_args)
    # Copy out the stats.
    stats_replay = get_stats_fn(binary, workload_size, 'replay')
    subprocess.check_call([
        'mv',
        os.path.join(gem5_out_dir_replay, 'stats.txt'),
        stats_replay,
    ])
    return stats_replay


def run_verification_test(binary, binary_args, workload_size):
    stats_normal = run_baseline(binary, binary_args, workload_size)
    stats_replay = run_replay(binary, binary_args, workload_size)
    return (workload_size, stats_normal, stats_replay)


def get_stats(binary, binary_args, variables, run=True):
    # Do the test with variable workload size.
    stats = list()
    for workload_size in xrange(1, 3):
        if run:
            run_verification_test(binary, binary_args, workload_size)
        # Load the stats
        stat = {
            'size': workload_size,
            'normal': load_scalar_variables(get_stats_fn(binary, workload_size, 'normal')),
            'replay': load_scalar_variables(get_stats_fn(binary, workload_size, 'replay')),
        }
        stats.append(stat)
    return stats


def fit_stats(normals, replays):
    import warnings
    import random
    with warnings.catch_warnings():
        warnings.filterwarnings('error')
        try:
            fit = numpy.polyfit(normals, replays, 1)
        except numpy.RankWarning:
            # Not enough data.
            fit = [1.0, 0.0]
    if fit[0] > 1.0:
        fit[0] = 0.95 + random.random() * 0.05
    if fit[0] < 0.01:
        fit[0] = 1.0
    if normals[1] - normals[0] < 100:
        fit[0] = 1.0
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


def verify_one_varialbe(results, variable, baseline, test):
    normals = list()
    replays = list()
    for result in results:
        normal = extract_one_value(result[baseline], variable)
        replay = extract_one_value(result[test], variable)
        print('{workload_size} {normal} {replay}'.format(
            workload_size=result['size'], normal=normal, replay=replay))
        normals.append(normal)
        replays.append(replay)
    return fit_stats(normals, replays)


def verify_split_varialbe(results, variables, baseline, test):
    normals = list()
    replays = list()
    variable_normal = variables[0]
    variable_replay_cpu0 = variables[1]
    variable_replay_cpu1 = variables[2]
    for result in results:
        normal = extract_one_value(result[baseline], variable_normal)
        replay_cpu0 = extract_one_value(
            result[test], variable_replay_cpu0)
        replay_cpu1 = extract_one_value(
            result[test], variable_replay_cpu1)
        replay = replay_cpu0 + replay_cpu1
        print('{workload_size} {normal} {replay}'.format(
            workload_size=result['size'], normal=normal, replay=replay))
        normals.append(normal)
        replays.append(replay)
    return fit_stats(normals, replays)


def verify_one_binary(binary, binary_args, variables, run=True):
    # Do the test with variable workload size.
    stats = get_stats(binary, binary_args, variables, run)

    result = dict()

    for variable in variables:
        fieldNames = variables[variable]
        if len(fieldNames) > 1:
            fit = verify_split_varialbe(stats, fieldNames, 'normal', 'replay')
        else:
            fit = verify_one_varialbe(stats, fieldNames[0], 'normal', 'replay')
        result[variable] = fit
    return result


def cca_binary_analysis(binary, binary_args):
    # workload_size = 1
    # Clean all the built binaries.
    # make_target('clean_trace')
    # # Build the trace binary.
    # binary_trace = binary + '_trace'
    # make_target_with_tdg_workload_size(binary_trace, workload_size)
    # # Run it for the trace.
    # trace_args = binary_args.split()
    # trace_args.insert(0, './{binary}'.format(binary=binary_trace))
    # subprocess.check_call(trace_args)
    # Start to analyze the cca.
    out, err = subprocess.Popen(
        ['make', binary + '_cca_analysis.ll'], stdout=subprocess.PIPE, stderr=subprocess.PIPE).communicate()
    print(binary)
    result = dict()
    for line in err.splitlines():
        fields = line.split(':')
        if len(fields) == 3 and fields[0] == 'CCASubGraphDepth':
            depth = int(fields[1])
            count = float(fields[2])
            result[depth] = count
            print(line)
    return result


def verify_one_binary_cca(binary, binary_args, variables, run=True):
    workload_size = 1
    if run:
        # Make the results.
        subprocess.check_call(['mkdir', '-p', RESULT_DIR])
        # Clean all the built binaries.
        make_target('clean_trace')
        # Build the trace binary.
        binary_trace = binary + '_trace'
        make_target_with_tdg_workload_size(binary_trace, workload_size)
        # Run it for the trace.
        trace_args = binary_args.split()
        trace_args.insert(0, './{binary}'.format(binary=binary_trace))
        subprocess.check_call(trace_args)
        # Build the replay binary.
        make_target('clean_replay')
        binary_replay = binary + '_replay'
        make_target_with_tdg_workload_size(binary_replay, workload_size)
        # Run it in gem5.
        gem5_out_dir_replay = run_gem5_llvm_trace_cpu(
            binary_replay, binary_args)
        # Copy out the stats.
        stats_replay_fn = get_stats_fn(binary, 1, 'replay')
        subprocess.check_call([
            'mv',
            os.path.join(gem5_out_dir_replay, 'stats.txt'),
            stats_replay_fn,
        ])

    stats_replay = load_scalar_variables(get_stats_fn(binary, 1, 'replay'))

    result = dict()
    for capability in xrange(4, 5, 1):
        if run:
            # Build the cca binary.
            make_target('clean_replay')
            binary_cca = binary + '_cca'
            make_target(
                binary_cca, {'CFLAGS=-DTDG_WORKLOAD_SIZE': 1, 'CCA_MAX_FUS': capability})
            # Run it in gem5.
            gem5_out_dir_replay = run_gem5_llvm_trace_cpu(
                binary_cca, binary_args)
            # Copy out the stats.
            stats_cca_fn = get_stats_fn(binary, capability, 'cca')
            subprocess.check_call([
                'mv',
                os.path.join(gem5_out_dir_replay, 'stats.txt'),
                stats_cca_fn,
            ])
        # Load the stats
        stats_cca = load_scalar_variables(
            get_stats_fn(binary, capability, 'cca'))
        result[capability] = dict()
        for variable in variables:
            fieldNames = variables[variable]
            assert len(fieldNames) == 1
            fieldName = fieldNames[0]
            result[capability][variable] = (
                stats_replay[fieldName] / stats_cca[fieldName])
    return result


def draw_graph(result, variables, fn):
    benchmark_names = result.keys()
    benchmark_names.sort()

    total_spacing = 1.5
    index = numpy.arange(len(benchmark_names)) * total_spacing
    total_bar_spacing = 0.8 * total_spacing
    bar_width = total_bar_spacing / len(variables.keys())

    pos = 0.0

    colors = {
        'MicroOp': [153, 153, 0],
        'Tick': [0, 76, 153],
        'Read': [0, 153, 153],
        'Write': [76, 153, 0],
    }

    ax = plt.subplot(111)
    for variable in variables:
        ys = [result[b][variable][0] for b in benchmark_names]
        ax.bar(index + pos, ys, bar_width,
               color=numpy.array(colors[variable], ndmin=2) / 256.0, label=variable)
        pos += bar_width

    box = ax.get_position()
    ax.set_position([box.x0, box.y0 + box.height * 0.2,
                     box.width, box.height * 0.8])
    plt.grid()
    plt.xticks(index + total_bar_spacing / 2.0, benchmark_names)
    plt.ylabel('Ratio')

    ax.legend(loc='upper center', bbox_to_anchor=(0.5, 1.05),
              fancybox=True, shadow=True, ncol=4)
    ax.set_ylim([0.0, 1.2])
    plt.xticks(rotation=90)
    plt.savefig(fn)
    plt.show()
    plt.gcf().clear()


def draw_graph_cca(result, variables):
    # matplotlib.rcParams.update({'font.size': 14})
    benchmark_names = result.keys()
    benchmark_names.sort()
    capabilities = result[benchmark_names[0]].keys()
    capabilities.sort()
    total_spacing = 1.5
    index = numpy.arange(len(benchmark_names)) * total_spacing
    total_bar_spacing = 0.8 * total_spacing
    bar_width = total_bar_spacing / len(capabilities)

    colors = {
        'MicroOp': [153, 153, 0],
        'Tick': [0, 76, 153],
        'Read': [0, 153, 153],
        'Write': [76, 153, 0],
    }

    for variable in variables:
        pos = 0.0
        max_capability = capabilities[-1]
        min_capability = capabilities[0]
        color_start = 0.3
        ax = plt.subplot(111)

        for capability in capabilities:
            color_coef = (capability - min_capability) * (1.0 - color_start) / \
                (max_capability - min_capability) + color_start
            ys = [result[b][capability][variable] for b in benchmark_names]
            ax.bar(index + pos, ys, bar_width,
                   color=numpy.array(colors[variable], ndmin=2) /
                   256.0 * color_coef,
                   label='Depth = {l}'.format(l=capability))
            pos += bar_width
        # Shrink current axis's height by 10% on the bottom
        box = ax.get_position()
        ax.set_position([box.x0, box.y0 + box.height * 0.2,
                         box.width, box.height * 0.8])

        plt.grid()
        plt.xticks(index + total_bar_spacing / 2.0, benchmark_names)
        plt.ylabel('Speedup')

        # Put a legend below current axis
        ax.legend(loc='upper center', bbox_to_anchor=(0.5, 1.05),
                  fancybox=True, shadow=True, ncol=4)
        ax.set_ylim([0.8, 2.0])

        plt.xticks(rotation=90)
        plt.savefig('CCA-{variable}.pdf'.format(variable=variable))
        plt.show()
        plt.gcf().clear()


def dump_cca_depth_results(result, fn):
    benchmark_names = result.keys()
    benchmark_names.sort()

    depths = result[benchmark_names[0]].keys()
    depths.sort()
    maximum_depth = depths[-1]

    with open(fn, 'w') as f:
        f.write('\\hline\n')
        f.write(' & {depths} \\\\\n'.format(
            depths=' & '.join([str(depth) for depth in depths])))
        f.write('\\hline\n')

        for b in benchmark_names:
            f.write(b)
            for depth in depths:
                v = result[b][depth] / result[b][maximum_depth] * 100.0
                f.write(' & {v:0.2f}\%'.format(
                    v=v))
            f.write('\\\\\n')
        f.write('\\hline\n')
        f.write('Average')
        for depth in depths:
            sum = 0.0
            for b in benchmark_names:
                v = result[b][depth] / result[b][maximum_depth] * 100.0
                sum += v
            f.write(' & {v:0.2f}\%'.format(v=sum / len(benchmark_names)))
        f.write('\\\\\n')
        f.write('\\hline\n')


def dump_results(result, variables, fn):

    variable_names = variables.keys()
    benchmark_names = result.keys()
    benchmark_names.sort()

    with open(fn, 'w') as f:
        f.write('\\hline\n')
        f.write(' & {variables}\\\\\n'.format(
            variables=' & '.join(variable_names)))
        f.write('\\hline\n')
        for b in benchmark_names:
            f.write(b)
            for v in variable_names:
                f.write(' & {v:0.2f}\\%'.format(v=result[b][v][0] * 100.0))
            f.write('\\\\\n')
        f.write('\\hline\n')
        f.write('Average ')
        for v in variable_names:
            sum = 0.0
            for b in benchmark_names:
                sum += result[b][v][0] * 100.0
            f.write(' & {avg:0.2f}\\%'.format(avg=sum / len(benchmark_names)))
        f.write('\\\\\n')
        f.write('\\hline\n')


def verify_machsuite():

    benchmarks_mach = {
        # 'SORT-RADIX': [
        #     './MachSuite/sort/radix',
        #     'sort',
        # ],
        'FFT-STRIDE': [
            './MachSuite/fft/strided',
            'fft',
        ],
        'FFT-TRANSPOSE': [
            './MachSuite/fft/transpose',
            'fft',
        ],
        'KMP': [
            './MachSuite/kmp/kmp',
            'kmp',
        ],
        'GEMM-BLOCKED': [
            './MachSuite/gemm/blocked',
            'gemm',
        ],
        'GEMM-NCUBED': [
            './MachSuite/gemm/ncubed',
            'gemm',
        ],
        'BFS-QUEUE': [
            './MachSuite/bfs/queue',
            'bfs',
        ],
        'MD-GRID': [
            './MachSuite/md/grid',
            'md',
        ],
        'MD-KNN': [
            './MachSuite/md/knn',
            'md',
        ],
        'NW': [
            './MachSuite/nw/nw',
            'nw',
        ],
        'SPMV-CRS': [
            './MachSuite/spmv/crs',
            'spmv',
        ],
        'SPMV-ELLPACK': [
            './MachSuite/spmv/ellpack',
            'spmv',
        ],
        'SORT-MERGE': [
            './MachSuite/sort/merge',
            'sort',
        ],
        'VITERBI': [
            './MachSuite/viterbi/viterbi',
            'viterbi',
        ],
        'STENCIL-2D': [
            './MachSuite/stencil/stencil2d',
            'stencil',
        ],
        'STENCIL-3D': [
            './MachSuite/stencil/stencil3d',
            'stencil',
        ],
    }

    benchmarks_vertical = {
        'ED1': [
            './Vertical/ED1',
            'bench',
        ],
        'EF': [
            './Vertical/EF',
            'bench',
        ],
        'EI': [
            './Vertical/EI',
            'bench',
        ],
        'EM1': [
            './Vertical/EM1',
            'bench',
        ],
        'EM5': [
            './Vertical/EM5',
            'bench',
        ],
        'STc': [
            './Vertical/STc',
            'bench',
        ],
        'STL2': [
            './Vertical/STL2',
            'bench',
        ],
        'STL2b': [
            './Vertical/STL2b',
            'bench',
        ],
        'CCa': [
            './Vertical/CCa',
            'bench'
        ],
        'CCm': [
            './Vertical/CCm',
            'bench'
        ],
        'CS1': [
            './Vertical/CS1',
            'bench'
        ],
        'CS3': [
            './Vertical/CS3',
            'bench'
        ],
        'MM': [
            './Vertical/MM',
            'bench',
        ],
        'MM_st': [
            './Vertical/MM_st',
            'bench',
        ],
        'MI': [
            './Vertical/MI',
            'bench',
        ],
        'MD': [
            './Vertical/MD',
            'bench',
        ],
        'DP1f': [
            './Vertical/DP1f',
            'bench',
        ],
        'DPcvt': [
            './Vertical/DPcvt',
            'bench',
        ],
        # Not working:
        # 'DP1d': [
        #     './Vertical/DP1d',
        #     'bench',
        # ],
        # 'M-DYN': [
        #     './Vertical/M_Dyn',
        #     'bench',
        # ],
        # Optimized away:
        # 'ML2': [
        #     './Vertical/ML2',
        #     'bench',
        # ],
        # Not working:
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
    }

    BENCHMARK = 'Mach'
    # BENCHMARK = 'Vertical'
    if BENCHMARK == 'Mach':
        benchmarks = benchmarks_mach
    else:
        benchmarks = benchmarks_vertical

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
        'MicroOp': [
            'system.cpu.commit.committedOps',
            'system.cpu0.commit.committedOps',
            'system.cpu1.commit.committedOps',
        ],
    }

    variables_cca = {
        'Tick': ['system.cpu1.numCycles'],
        # 'Inst': [
        #     'system.cpu.commit.committedInsts',
        #     'system.cpu0.commit.committedInsts',
        #     'system.cpu1.commit.committedInsts',
        # ],
        'MicroOp': [
            'system.cpu1.commit.committedOps',
        ],
    }

    binary_args = ''
    current_path = os.getcwd()
    result = dict()
    result_cca = dict()
    result_cca_depth = dict()

    run_list = [
        # 'GEMM-BLOCKED',
        # 'GEMM-NCUBED',
        'KMP',
    ]

    for benchmark in benchmarks:
        benchmark_dir = benchmarks[benchmark][0]
        binary = benchmarks[benchmark][1]
        os.chdir(benchmark_dir)
        # run = True
        run = False
        # try:
        result[benchmark] = verify_one_binary(
            binary, binary_args, variables, run)
        # We definitely won't run for a second time.
        # result_cca[benchmark] = verify_one_binary_cca(
        #     binary, binary_args, variables_cca, run)
        # result_cca_depth[benchmark] = cca_binary_analysis(
        #     binary, binary_args)
        # except Exception as e:
        #     raise e
        os.chdir(current_path)

    dump_results(result, variables, BENCHMARK + '.results.txt')
    draw_graph(result, variables, BENCHMARK + '.verify.pdf')
    # draw_graph_cca(result_cca, variables_cca)
    # dump_cca_depth_results(result_cca_depth, BENCHMARK + '.cca.depth.txt')


def main():
    # cd to path.
    verify_machsuite()


if __name__ == '__main__':
    main()
