import os
import subprocess

import Constants as C
import Util


class Benchmark(object):

    def __init__(self, name, raw_bc, links, args=None, trace_func=None,
                 trace_lib='Protobuf', lang='C'):
        self.name = name
        self.raw_bc = raw_bc
        self.links = links
        self.args = args
        self.trace_func = trace_func
        self.lang = lang

        self.pass_so = os.path.join(
            C.LLVM_TDG_BUILD_DIR, 'libLLVMTDGPass.so')

        if trace_lib == 'Protobuf':
            self.trace_links = links + [
                C.PROTOBUF_LIB,
                C.LIBUNWIND_LIB,
            ]
            self.trace_lib = os.path.join(
                C.LLVM_TDG_BUILD_DIR, 'trace/libTracerProtobuf.a'
            )
            self.trace_format = 'protobuf'
        else:
            self.trace_links = links + [
                '-lz',
                C.LIBUNWIND_LIB,
            ]
            self.trace_lib = os.path.join(
                C.LLVM_TDG_BUILD_DIR, 'trace/libTracerGZip.a'
            )
            self.trace_format = 'gzip'

    def clean(self):
        clean_cmd = [
            'rm',
            '-f',
            self.get_trace_bin(),
            self.get_trace_bc(),
            self.get_replay_bin(),
            self.get_replay_bc(),
        ]
        Util.call_helper(clean_cmd)

    def get_name(self):
        assert('get_name is not implemented by derived class')

    def get_trace_bc(self):
        return '{name}.traced.bc'.format(name=self.get_name())

    def get_trace_bin(self):
        return '{name}.traced.exe'.format(name=self.get_name())

    def get_replay_bc(self):
        return '{name}.replay.bc'.format(name=self.get_name())

    def get_replay_bin(self):
        return '{name}.replay.exe'.format(name=self.get_name())

    def get_profile(self):
        return '{name}.profile'.format(name=self.get_name())

    def get_traces(self):
        traces = list()
        name = self.get_name()
        for i in xrange(self.get_n_traces()):
            if name.endswith('gcc_s') and i == 7:
                # So far there is a bug causing tdg #7 not transformable.
                continue
            traces.append('{run}/{name}.{i}.trace'.format(
                run=self.get_run_path(),
                name=name,
                i=i))
        return traces

    def get_tdgs(self, transform_pass):
        tdgs = list()
        name = self.get_name()
        for i in xrange(self.get_n_traces()):
            if name.endswith('gcc_s') and i == 7:
                # So far there is a but causing tdg #7 not transformable.
                continue
            tdgs.append('{run}/{name}.{transform_pass}.{i}.tdg'.format(
                run=self.get_run_path(),
                name=name,
                transform_pass=transform_pass,
                i=i))
        return tdgs

    def get_results(self, transform_pass):
        results = list()
        name = self.get_name()
        Util.call_helper(
            ['mkdir', '-p', C.LLVM_TDG_RESULT_DIR])
        for i in xrange(self.get_n_traces()):
            if name.endswith('gcc_s') and i == 7:
                # So far there is a but causing tdg #7 not transformable.
                continue
            results.append(
                os.path.join(
                    C.LLVM_TDG_RESULT_DIR,
                    '{name}.{transform_pass}.{i}.txt'.format(
                        name=name, transform_pass=transform_pass, i=i
                    )))
        return results

    """
    Generate the trace.
    """

    def run_trace(self, trace_file='llvm_trace'):
        # Remember to set the environment for trace.
        os.putenv('LLVM_TDG_TRACE_FILE', trace_file)
        run_cmd = [
            './' + self.get_trace_bin(),
        ]
        if self.args is not None:
            run_cmd += self.args
        print('# Run traced binary...')
        Util.call_helper(run_cmd)

    """
    Construct the traced binary.
    """

    def build_trace(self, link_stdlib=False, trace_reachable_only=False, debugs=[]):
        trace_cmd = [
            C.OPT,
            '-load={PASS_SO}'.format(PASS_SO=self.pass_so),
            '-trace-pass',
            self.raw_bc,
            '-o',
            self.get_trace_bc(),
        ]
        if self.trace_func is not None and len(self.trace_func) > 0:
            trace_cmd.append('-trace-function=' + self.trace_func)
        if trace_reachable_only:
            trace_cmd.append('-trace-reachable-only=1')
        if debugs:
            print(debugs)
            trace_cmd.append(
                '-debug-only={debugs}'.format(debugs=','.join(debugs)))
        print('# Instrumenting tracer...')
        Util.call_helper(trace_cmd)
        if link_stdlib:
            link_cmd = [
                C.CXX,
                '-O3',
                '-nostdlib',
                '-static',
                self.get_trace_bc(),
                self.trace_lib,
                C.MUSL_LIBC_STATIC_LIB,
                '-lc++',
                '-o',
                self.get_trace_bin(),
            ]
        else:
            link_cmd = [
                C.CXX,
                '-O3',
                self.get_trace_bc(),
                self.trace_lib,
                '-o',
                self.get_trace_bin(),
            ]
        link_cmd += self.trace_links
        print('# Link to traced binary...')
        Util.call_helper(link_cmd)

    """
    Construct the replay binary from the trace.
    """

    def build_replay(self,
                     pass_name='replay',
                     trace_file='llvm.0.trace',
                     profile_file='llvm.profile',
                     tdg_detail='integrated',
                     output_tdg=None,
                     debugs=[]
                     ):
        opt_cmd = [
            C.OPT,
            '-load={PASS_SO}'.format(PASS_SO=self.pass_so),
            '-{pass_name}'.format(pass_name=pass_name),
            '-trace-file={trace_file}'.format(trace_file=trace_file),
            '-tdg-profile-file={profile_file}'.format(
                profile_file=profile_file),
            '-trace-format={format}'.format(format=self.trace_format),
            '-datagraph-detail={detail}'.format(detail=tdg_detail),
            self.raw_bc,
            '-o',
            self.get_replay_bc(),
        ]
        if output_tdg is not None:
            opt_cmd.append('-output-datagraph=' + output_tdg)
        if debugs:
            opt_cmd.append(
                '-debug-only={debugs}'.format(debugs=','.join(debugs)))
        print('# Processing trace...')
        Util.call_helper(opt_cmd)
        build_cmd = [
            C.CC if self.lang == 'C' else C.CXX,
            '-static',
            '-o',
            self.get_replay_bin(),
            '-I{gem5_include}'.format(gem5_include=C.GEM5_INCLUDE_DIR),
            C.GEM5_M5OPS_X86,
            C.LLVM_TDG_REPLAY_C,
            self.get_replay_bc(),
        ]
        build_cmd += self.links
        print('# Building replay binary...')
        Util.call_helper(build_cmd)

    """
    Get trace statistics.
    """

    def get_trace_statistics(self,
                             trace_file,
                             profile_file,
                             debugs=[]
                             ):
        opt_cmd = [
            C.OPT,
            '-load={PASS_SO}'.format(PASS_SO=self.pass_so),
            '-trace-statistic-pass',
            '-trace-file={trace_file}'.format(trace_file=trace_file),
            '-trace-format={format}'.format(format=self.trace_format),
            '-tdg-profile-file={profile_file}'.format(
                profile_file=profile_file),
            # For statistics, simple is enough.
            '-datagraph-detail=simple',
            self.raw_bc,
            '-analyze',
        ]
        if debugs:
            opt_cmd.append(
                '-debug-only={debugs}'.format(debugs=','.join(debugs)))
        print('# Collecting statistics trace...')
        Util.call_helper(opt_cmd)

    """
    Replay the binary with gem5.

    Returns
    -------
    The gem5 output directory (abs path).
    """

    def gem5_replay(self, standalone=0, output_tdg=None, debugs=[]):
        LLVM_TRACE_FN = 'llvm_trace_gem5.txt'
        if output_tdg is not None:
            LLVM_TRACE_FN = output_tdg
        GEM5_OUT_DIR = '{cpu_type}.replay.{tdg}'.format(
            cpu_type=C.CPU_TYPE, tdg=output_tdg)
        Util.call_helper(['mkdir', '-p', GEM5_OUT_DIR])
        gem5_args = [
            C.GEM5_X86,
            '--outdir={outdir}'.format(outdir=GEM5_OUT_DIR),
            C.GEM5_LLVM_TRACE_SE_CONFIG,
            '--cmd={cmd}'.format(cmd=self.get_replay_bin()),
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
        if debugs:
            gem5_args.insert(
                1, '--debug-flags={flags}'.format(flags=','.join(debugs)))
        if self.args is not None:
            gem5_args.append(
                '--options={binary_args}'.format(binary_args=' '.join(self.args)))
        print('# Replaying the datagraph...')
        Util.call_helper(gem5_args)
        return os.path.join(os.getcwd(), GEM5_OUT_DIR)
