import os
import subprocess

import Constants as C


class Benchmark:

    def __init__(self, name, raw_bc, links, args=None, trace_func=None):
        self.name = name
        self.raw_bc = raw_bc
        self.links = links
        self.args = args
        self.trace_func = trace_func

        self.pass_so = os.path.join(
            C.LLVM_TDG_DIR, 'build', 'prototype', 'libLLVMTDGPass.so')

        self.trace_bin = name + '.traced'
        # self.trace_links = links + ['-lprotobuf']
        # self.trace_lib = os.path.join(
        #     C.LLVM_TDG_DIR, 'build', 'prototype', 'libTracerProtobuf.a'
        # )
        # self.trace_format = 'protobuf'
        self.trace_links = links + ['-lz']
        self.trace_lib = os.path.join(
            C.LLVM_TDG_DIR, 'build', 'prototype', 'libTracerGZip.a'
        )
        self.trace_format = 'gzip'

        self.gem5_pseudo = os.path.join(
            C.GEM5_DIR, 'util', 'm5', 'm5op_x86.S')
        self.replay_c = os.path.join(
            C.LLVM_TDG_DIR, 'benchmark', 'replay.c')
        self.replay_bin = name + '.replay'

    """
    Compile and generate the trace.
    """

    def trace(self):
        trace_bc = self.trace_bin + '.bc'
        trace_cmd = [
            'opt',
            '-load={PASS_SO}'.format(PASS_SO=self.pass_so),
            '-trace-pass',
            self.raw_bc,
            '-o',
            trace_bc,
        ]
        if self.trace_func is not None:
            trace_cmd.append('-trace-function=' + self.trace_func)
        print('# Instrumenting tracer...')
        subprocess.check_call(trace_cmd)
        link_cmd = [
            'clang++',
            '-O0',
            trace_bc,
            self.trace_lib,
            '-o',
            self.trace_bin,
        ]
        link_cmd += self.trace_links
        print('# Link to traced binary...')
        subprocess.check_call(link_cmd)
        run_cmd = [
            './' + self.trace_bin,
        ]
        if self.args is not None:
            run_cmd += self.args
        print('# Run traced binary...')
        subprocess.check_call(run_cmd)

    """
    Construct the replay binary from the trace.
    """

    def build_replay(self):
        replay_bc = self.replay_bin + '.bc'
        build_cmd = [
            'opt',
            '-load={PASS_SO}'.format(PASS_SO=self.pass_so),
            '-replay',
            '-trace-file=llvm_trace',
            '-trace-format={format}'.format(format=self.trace_format),
            self.raw_bc,
            '-o',
            replay_bc,
            '-debug-only=DataGraph',
        ]
        print('# Processing trace...')
        subprocess.check_call(build_cmd)
        # Compile the bitcode to object file, as gem5's pseudo-inst
        # can only be linked with gcc.
        replay_o = self.replay_bin + '.o'
        compile_replay_bc_cmd = [
            'clang',
            '-c',
            '-O0',
            replay_bc,
            '-o',
            replay_o,
        ]
        print('# Compiling bitcode to object file...')
        subprocess.check_call(compile_replay_bc_cmd)
        # Link to replay binary.
        link_cmd = [
            'gcc',
            '-O0',
            '-I{gem5_include}'.format(gem5_include=C.GEM5_INCLUDE_DIR),
            self.gem5_pseudo,
            self.replay_c,
            replay_o,
            '-o',
            self.replay_bin
        ]
        link_cmd += self.links
        print('# Linking the replay binary...')
        subprocess.check_call(link_cmd)


    """
    Replay the binary with gem5.

    Returns
    -------
    The gem5 output directory (abs path).
    """

    def gem5_replay(self, standalone=0):
        LLVM_TRACE_FN = 'llvm_trace_gem5.txt'
        GEM5_OUT_DIR = '{cpu_type}.replay'.format(cpu_type=C.CPU_TYPE)
        subprocess.check_call(['mkdir', '-p', GEM5_OUT_DIR])
        gem5_args = [
            C.GEM5_X86,
            '--outdir={outdir}'.format(outdir=GEM5_OUT_DIR),
            '--debug-flags=LLVMTraceCPU',
            C.GEM5_LLVM_TRACE_SE_CONFIG,
            '--cmd={cmd}'.format(cmd=self.replay_bin),
            '--llvm-standalone={standlone}'.format(standlone=standalone),
            '--llvm-trace-file={trace_file}'.format(trace_file=LLVM_TRACE_FN),
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
        ]
        if self.args is not None:
            gem5_args.append(
                '--options={binary_args}'.format(binary_args=' '.join(self.args)))
        print('# Replaying the datagraph...')
        subprocess.check_call(gem5_args)
        return os.path.join(os.getcwd(), GEM5_OUT_DIR)
