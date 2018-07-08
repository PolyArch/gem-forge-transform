import os
import subprocess

import Constants as C
import Util


class Benchmark(object):

    def __init__(self, name, raw_bc, links, args=None, trace_func=None,
                 trace_lib='Protobuf', skip_inst=0):
        self.name = name
        self.raw_bc = raw_bc
        self.links = links
        self.args = args
        self.trace_func = trace_func
        self.skip_inst = skip_inst

        self.pass_so = os.path.join(
            C.LLVM_TDG_BUILD_DIR, 'libLLVMTDGPass.so')

        self.trace_bin = name + '.traced'
        self.trace_bc = self.trace_bin + '.bc'
        if trace_lib == 'Protobuf':
            self.trace_links = links + [C.PROTOBUF_ELLCC_LIB]
            self.trace_lib = os.path.join(
                C.LLVM_TDG_BUILD_DIR, 'trace/libTracerProtobuf.a'
            )
            self.trace_format = 'protobuf'
        else:
            self.trace_links = links + ['-lz']
            self.trace_lib = os.path.join(
                C.LLVM_TDG_BUILD_DIR, 'trace/libTracerGZip.a'
            )
            self.trace_format = 'gzip'

        self.gem5_pseudo = os.path.join(
            C.GEM5_DIR, 'util', 'm5', 'm5op_x86.S')
        self.replay_c = os.path.join(
            C.LLVM_TDG_DIR, 'benchmark', 'replay.c')
        self.replay_bin = name + '.replay'
        self.replay_bc = self.replay_bin + '.bc'

    def clean(self):
        clean_cmd = [
            'rm',
            '-f',
            self.trace_bin,
            self.trace_bc,
            self.replay_bin,
            self.replay_bc,
        ]
        Util.call_helper(clean_cmd)

    """
    Generate the trace.
    """

    def run_trace(self, trace_file='llvm_trace'):
        # Remember to set the environment for trace.
        os.putenv('LLVM_TDG_SKIP_INST', str(self.skip_inst))
        os.putenv('LLVM_TDG_TRACE_FILE', trace_file)
        run_cmd = [
            './' + self.trace_bin,
        ]
        if self.args is not None:
            run_cmd += self.args
        print('# Run traced binary...')
        Util.call_helper(run_cmd)

    """
    Construct the traced binary.
    """

    def build_trace(self):
        trace_cmd = [
            'opt',
            '-load={PASS_SO}'.format(PASS_SO=self.pass_so),
            '-trace-pass',
            self.raw_bc,
            '-o',
            self.trace_bc,
        ]
        if self.trace_func is not None:
            trace_cmd.append('-trace-function=' + self.trace_func)
        print('# Instrumenting tracer...')
        Util.call_helper(trace_cmd)
        link_cmd = [
            'ecc++',
            '-O0',
            self.trace_bc,
            self.trace_lib,
            '-o',
            self.trace_bin,
        ]
        link_cmd += self.trace_links
        print('# Link to traced binary...')
        Util.call_helper(link_cmd)

    """
    Construct the replay binary from the trace.
    """

    def build_replay(self, pass_name='replay', trace_file='llvm_trace', tdg_detail='integrated', debugs=[]):
        opt_cmd = [
            'opt',
            '-load={PASS_SO}'.format(PASS_SO=self.pass_so),
            '-{pass_name}'.format(pass_name=pass_name),
            '-trace-file={trace_file}'.format(trace_file=trace_file),
            '-trace-format={format}'.format(format=self.trace_format),
            '-datagraph-detail={detail}'.format(detail=tdg_detail),
            self.raw_bc,
            '-o',
            self.replay_bc,
        ]
        if debugs:
            opt_cmd.append(
                '-debug-only={debugs}'.format(debugs=','.join(debugs)))
        print('# Processing trace...')
        Util.call_helper(opt_cmd)
        build_cmd = [
            'ecc',
            '-static',
            '-o',
            self.replay_bin,
            '-I{gem5_include}'.format(gem5_include=C.GEM5_INCLUDE_DIR),
            self.gem5_pseudo,
            self.replay_c,
            self.replay_bc,
        ]
        build_cmd += self.links
        print('# Building replay binary...')
        Util.call_helper(build_cmd)

    """
    Replay the binary with gem5.

    Returns
    -------
    The gem5 output directory (abs path).
    """

    def gem5_replay(self, standalone=0):
        LLVM_TRACE_FN = 'llvm_trace_gem5.txt'
        GEM5_OUT_DIR = '{cpu_type}.replay'.format(cpu_type=C.CPU_TYPE)
        Util.call_helper(['mkdir', '-p', GEM5_OUT_DIR])
        gem5_args = [
            C.GEM5_X86,
            '--outdir={outdir}'.format(outdir=GEM5_OUT_DIR),
            # '--debug-flags=LLVMTraceCPU',
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
        Util.call_helper(gem5_args)
        return os.path.join(os.getcwd(), GEM5_OUT_DIR)
