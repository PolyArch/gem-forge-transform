
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
        '--options={binary_args}'.format(binary_args=binary_args),
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
        '--options={binary_args}'.format(binary_args=binary_args),
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
    trace_args = binary_args.split()
    trace_args.insert(0, './{binary}'.format(binary=binary_trace))
    subprocess.check_call(trace_args)
    # Build the replay binary.
    binary_replay = binary + '_replay'
    make_target(binary_replay, workload_size)
    # Run it in gem5.
    gem5_out_dir_replay = run_gem5_llvm_trace_cpu(binary_replay, binary_args)
    gem5_stats_replay = load_scalar_variables(
        os.path.join(gem5_out_dir_replay, GEM5_STATS_FN))
    return (workload_size, gem5_stats_normal, gem5_stats_replay)


def fit_stats(normals, replays):
    fit = numpy.polyfit(normals, replays, 1)
    print(
        'fit {a}*normal + {b}'.format(a=fit[0], b=fit[1]))
    fit_fn = numpy.poly1d(fit)
    plt.plot(normals, replays, 'yo',
             normals, fit_fn(normals), '--k')
    plt.show()


def verify_one_varialbe(results, variable):
    normals = list()
    replays = list()
    for result in results:
        normal = result[1][variable]
        replay = result[2][variable]
        print('{workload_size} {normal} {replay}'.format(
            workload_size=result[0], normal=normal, replay=replay))
        normals.append(normal)
        replays.append(replay)
    fit_stats(normals, replays)


def verify_split_varialbe(results, variable_normal, variable_replay_cpu0, variable_replay_cpu1):
    normals = list()
    replays = list()
    for result in results:
        normal = result[1][variable_normal]
        replay = result[2][variable_replay_cpu0] + \
            result[2][variable_replay_cpu1]
        print('{workload_size} {normal} {replay}'.format(
            workload_size=result[0], normal=normal, replay=replay))
        normals.append(normal)
        replays.append(replay)
    fit_stats(normals, replays)


def verify(binary, binary_args):
    # Do the test with variable workload size.
    results = list()
    for workload_size in xrange(1, 5):
        results.append(run_single_test(binary, binary_args, workload_size))
    verify_one_varialbe(results, 'sim_ticks')
    # verify_one_varialbe(results, 'system.mem_ctrls.bytes_read::total')
    # verify_one_varialbe(results, 'system.mem_ctrls.bytes_written::total')
    # verify_split_varialbe(results, 'system.cpu.dcache.WriteReq_accesses::total',
    #                       'system.cpu0.dcache.WriteReq_accesses::total', 'system.cpu1.dcache.WriteReq_accesses::total')
    # verify_split_varialbe(results, 'system.cpu.dcache.ReadReq_accesses::total',
    #                       'system.cpu0.dcache.ReadReq_accesses::total', 'system.cpu1.dcache.ReadReq_accesses::total')
    verify_split_varialbe(results, 'system.cpu.commit.op_class_0::IntAlu',
                          'system.cpu0.commit.op_class_0::IntAlu', 'system.cpu1.iew.FU_type_0::IntAlu')
    verify_split_varialbe(results, 'system.cpu.commit.committedInsts',
                          'system.cpu0.commit.committedInsts', 'system.cpu1.commit.committedInsts')
    verify_split_varialbe(results, 'system.cpu.commit.committedOps',
                          'system.cpu0.commit.committedOps', 'system.cpu1.commit.committedOps')


def main(path, binary, binary_args):
    # cd to path.
    os.chdir(path)
    verify(binary, binary_args)


if __name__ == '__main__':
    # main('./hello', 'hello', '')
    main('./Vertical/EI', 'bench', '')
    # main('./MachSuite/fft/strided', 'fft', 'input.data check.data')
    # main('./MachSuite/fft/transpose', 'fft', 'input.data output.data')
    # main('./MachSuite/kmp/kmp', 'kmp', 'input.data output.data')
