
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


def make_target(target, workload_size):
    subprocess.check_call(
        ['make', 'CFLAGS=-DTDG_WORKLOAD_SIZE={workload_size}'.format(workload_size=workload_size), target])


def run_gem5(binary, binary_args):
    CPU_TYPE = 'DerivO3CPU'
    GEM5_OUT_DIR = CPU_TYPE
    GEM5_SE_CONFIG = GEM5_PATH + '/configs/example/se.py'
    subprocess.check_call(['mkdir', '-p', GEM5_OUT_DIR])
    gem5_args = [
        GEM5_X86,
        '--outdir={outdir}'.format(outdir=GEM5_OUT_DIR),
        GEM5_SE_CONFIG,
        '--cmd={cmd}'.format(cmd=binary),
        '--options="{binary_args}"'.format(binary_args=binary_args),
        '--caches',
        '--l2cache',
        '--cpu-type={cpu_type}'.format(cpu_type=CPU_TYPE),
        '--num-cpus=1',
        '--l1d_size={l1d_size}'.format(l1d_size=GEM5_L1D_SIZE),
        '--l1i_size={l1i_size}'.format(l1i_size=GEM5_L1I_SIZE),
        '--l2_size={l2_size}'.format(l2_size=GEM5_L2_SIZE),
        '--l3_size={l3_size}'.format(l3_size=GEM5_L3_SIZE),
    ]
    subprocess.check_call(gem5_args)
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
        '--options="{binary_args}"'.format(binary_args=binary_args),
        '--caches',
        '--l2cache',
        '--cpu-type={cpu_type}'.format(cpu_type=CPU_TYPE),
        '--num-cpus=1',
        '--l1d_size={l1d_size}'.format(l1d_size=GEM5_L1D_SIZE),
        '--l1i_size={l1i_size}'.format(l1i_size=GEM5_L1I_SIZE),
        '--l2_size={l2_size}'.format(l2_size=GEM5_L2_SIZE),
        '--l3_size={l3_size}'.format(l3_size=GEM5_L3_SIZE),
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


def run_single_test(binary, binary_args, workload_size):
    # Clean all the built binaries.
    make_target('clean', 0)
    make_target('clean_trace', 0)
    make_target('clean_replay', 0)
    # Build normal binary.
    make_target(binary, workload_size)
    # Run it in gem5.
    gem5_out_dir_normal = run_gem5(binary, binary_args)
    gem5_stats_normal = load_scalar_variables(
        os.path.join(gem5_out_dir_normal, GEM5_STATS_FN))
    # Build the trace binary.
    binary_trace = binary + '_trace'
    make_target(binary_trace, workload_size)
    # Run it for the trace.
    subprocess.check_call(
        ['./{binary}'.format(binary=binary_trace), binary_args])
    # Build the replay binary.
    binary_replay = binary + '_replay'
    make_target(binary_replay, workload_size)
    # Run it in gem5.
    gem5_out_dir_replay = run_gem5_llvm_trace_cpu(binary_replay, binary_args)
    gem5_stats_replay = load_scalar_variables(
        os.path.join(gem5_out_dir_replay, GEM5_STATS_FN))
    return (workload_size, gem5_stats_normal, gem5_stats_replay)


def verify(binary, binary_args):
    # Do the test with variable workload size.
    results = list()
    for workload_size in xrange(1, 10):
        results.append(run_single_test(binary, binary_args, workload_size))
    SIM_TICKS_FIELD = 'sim_ticks'
    sim_ticks_normals = list()
    sim_ticks_replays = list()
    for result in results:
        sim_ticks_normal = result[1][SIM_TICKS_FIELD]
        sim_ticks_replay = result[2][SIM_TICKS_FIELD]
        print('{workload_size} {normal} {replay}'.format(
            workload_size=result[0], normal=sim_ticks_normal, replay=sim_ticks_replay))
        sim_ticks_normals.append(sim_ticks_normal)
        sim_ticks_replays.append(sim_ticks_replay)
    sim_ticks_fit = numpy.polyfit(sim_ticks_normals, sim_ticks_replays, 1)
    print(
        'fit {a}*normal + {b}'.format(a=sim_ticks_fit[0], b=sim_ticks_fit[1]))
    sim_ticks_fit_fn = numpy.poly1d(sim_ticks_fit)
    plt.plot(sim_ticks_normals, sim_ticks_replays, 'yo',
             sim_ticks_normals, sim_ticks_fit_fn(sim_ticks_normals), '--k')
    plt.show()


def main(path, binary, binary_args):
    # cd to path.
    os.chdir(path)
    verify(binary, binary_args)


if __name__ == '__main__':
    main('./hello', 'hello', '')
