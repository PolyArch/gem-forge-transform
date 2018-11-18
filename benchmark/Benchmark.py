import os
import subprocess

import Constants as C
import Util
import Utils.Gem5ConfigureManager


class Benchmark(object):

    def __init__(self, name, raw_bc, links, args=None, trace_func=None,
                 trace_lib='Protobuf', lang='C', standalone=1):
        # Initialize the result directory.
        Util.call_helper(
            ['mkdir', '-p', C.LLVM_TDG_RESULT_DIR])
        self.name = name
        self.raw_bc = raw_bc
        self.links = links
        self.args = args
        self.trace_func = trace_func
        self.lang = lang
        self.standalone = standalone

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

    def debug(self, msg):
        print('> {name} <: {msg}'.format(name=self.get_name(), msg=msg))

    def get_name(self):
        assert('get_name is not implemented by derived class')

    def get_trace_bc(self):
        return '{name}.traced.bc'.format(name=self.get_name())

    def get_inst_uid(self):
        return '{name}.inst.uid.txt'.format(name=self.get_name())

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

    def get_tdgs(self, transform_config):
        tdgs = list()
        name = self.get_name()
        for i in xrange(self.get_n_traces()):
            if name.endswith('gcc_s') and i == 7:
                # So far there is a but causing tdg #7 not transformable.
                continue
            tdgs.append('{run}/{name}.{transform_id}.{i}.tdg'.format(
                run=self.get_run_path(),
                name=name,
                transform_id=transform_config.get_id(),
                i=i))
        return tdgs

    def get_results(self, transform_config):
        results = list()
        name = self.get_name()
        tdgs = self.get_tdgs(transform_config)
        for tdg in tdgs:
            results.append(self.gem5_config.get_result(tdg))
        return results
        # raise ValueError('Not implemented')

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
            '-trace-inst-uid-file',
            self.get_inst_uid(),
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
                # '-O0',
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
                     transform_config,
                     trace_file='llvm.0.trace',
                     profile_file='llvm.profile',
                     tdg_detail='integrated',
                     output_tdg=None,
                     debugs=[]
                     ):
        opt_cmd = [
            C.OPT,
            '-load={PASS_SO}'.format(PASS_SO=self.pass_so),
        ]
        transform_options = transform_config.get_options()
        opt_cmd += transform_options
        opt_cmd += [
            '-trace-file={trace_file}'.format(trace_file=trace_file),
            '-datagraph-inst-uid-file={inst_uid}'.format(
                inst_uid=self.get_inst_uid()),
            '-tdg-profile-file={profile_file}'.format(
                profile_file=profile_file),
            '-trace-format={format}'.format(format=self.trace_format),
            '-datagraph-detail={detail}'.format(detail=tdg_detail),
            self.raw_bc,
            '-o',
            self.get_replay_bc(),
        ]
        if output_tdg is not None:
            output_extra_folder = os.path.join(
                os.getcwd(), output_tdg + '.extra')
            if os.path.exists(output_extra_folder):
                try:
                    Util.call_helper(['rm', '-r', output_extra_folder])
                except Exception as e:
                    pass
            if not os.path.exists(output_extra_folder):
                os.mkdir(output_extra_folder)
            opt_cmd.append('-output-datagraph=' + output_tdg)
            opt_cmd.append('-output-extra-folder-path=' + output_extra_folder)
        else:
            assert(False)
        if debugs:
            opt_cmd.append(
                '-debug-only={debugs}'.format(debugs=','.join(debugs)))
        print('# Processing trace...')
        Util.call_helper(opt_cmd)
        if tdg_detail == 'integrated':
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
    Prepare the gem5 simulate command.
    """

    def get_gem5_simulate_command(
            self,
            tdg,
            gem5_config,
            replay_bin,
            gem5_out_dir,
            debugs=[],
            hoffman2=False):
        gem5_args = [
            C.GEM5_X86 if not hoffman2 else C.HOFFMAN2_GEM5_X86,
            '--outdir={outdir}'.format(outdir=gem5_out_dir),
            C.GEM5_LLVM_TRACE_SE_CONFIG if not hoffman2 else C.HOFFMAN2_GEM5_LLVM_TRACE_SE_CONFIG,
            '--cmd={cmd}'.format(cmd=replay_bin),
            '--llvm-standalone={standlone}'.format(standlone=self.standalone),
            '--llvm-trace-file={trace_file}'.format(trace_file=tdg),
            '--llvm-issue-width={ISSUE_WIDTH}'.format(
                ISSUE_WIDTH=C.ISSUE_WIDTH),
            '--llvm-store-queue-size={STORE_QUEUE_SIZE}'.format(
                STORE_QUEUE_SIZE=C.STORE_QUEUE_SIZE),
            '--llvm-mcpat={use_mcpat}'.format(use_mcpat=C.GEM5_USE_MCPAT),
            '--caches',
            '--l2cache',
            '--cpu-type={cpu_type}'.format(cpu_type=C.CPU_TYPE),
            '--num-cpus=1',
            '--l1d_size={l1d_size}'.format(l1d_size=C.GEM5_L1D_SIZE),
            '--l1i_size={l1i_size}'.format(l1i_size=C.GEM5_L1I_SIZE),
            '--l2_size={l2_size}'.format(l2_size=C.GEM5_L2_SIZE),
        ]
        if gem5_config.prefetch:
            gem5_args.append(
                '--llvm-prefetch=1'
            )
        if gem5_config.stream_engine_is_oracle:
            gem5_args.append(
                '--gem-forge-stream-engine-is-oracle=1'
            )
        if gem5_config.stream_engine_max_run_ahead_length is not None:
            gem5_args.append(
                '--gem-forge-stream-engine-max-run-ahead-length={x}'.format(
                    x=gem5_config.stream_engine_max_run_ahead_length
                )
            )
        if gem5_config.stream_engine_throttling is not None:
            gem5_args.append(
                '--gem-forge-stream-engine-throttling={x}'.format(
                    x=gem5_config.stream_engine_throttling
                )
            )

        if gem5_config.stream_engine_enable_coalesce == 'coalesce':
            gem5_args.append(
                '--gem-forge-stream-engine-enable-coalesce=1'
            )
        elif gem5_config.stream_engine_enable_coalesce == 'merge':
            # Enable merge also enables coalesce.
            gem5_args.append(
                '--gem-forge-stream-engine-enable-coalesce=1'
            )
            gem5_args.append(
                '--gem-forge-stream-engine-enable-merge=1'
            )
        if gem5_config.stream_engine_l1d != 'original':
            gem5_args.append(
                '--gem-forge-stream-engine-l1d={l1d}'.format(
                    l1d=gem5_config.stream_engine_l1d)
            )

        if debugs:
            gem5_args.insert(
                1, '--debug-flags={flags}'.format(flags=','.join(debugs)))
        if self.args is not None:
            gem5_args.append(
                '--options={binary_args}'.format(binary_args=' '.join(self.args)))
        return gem5_args

    """
    Replay the binary with gem5.

    Returns
    -------
    The gem5 output directory (abs path).
    """

    def gem5_replay(self, tdg, gem5_config, debugs=[]):
        print gem5_config
        print tdg
        gem5_out_dir = gem5_config.get_gem5_dir(tdg)
        print gem5_out_dir
        Util.call_helper(['mkdir', '-p', gem5_out_dir])
        print 'what?'
        gem5_args = self.get_gem5_simulate_command(
            tdg, gem5_config, self.get_replay_bin(), gem5_out_dir, debugs, False)
        print('# Replaying the datagraph...')
        Util.call_helper(gem5_args)
        return gem5_out_dir

    """
    Simulate the datagraph with gem5.
    """

    def simulate(self, tdg, gem5_config, debugs):
        print('# Simulating the datagraph')
        gem5_outdir = self.gem5_replay(
            tdg=tdg,
            gem5_config=gem5_config,
            debugs=debugs,
        )
        # Util.call_helper([
        #     'cp',
        #     os.path.join(gem5_outdir, 'region.stats.txt'),
        #     result,
        # ])

    def simulate_hoffman2(self, tdg, gem5_config, result, scp=True, debugs=[]):
        _, tdg_name = os.path.split(tdg)
        hoffman2_ssh_tdg = os.path.join(C.HOFFMAN2_SSH_SCRATCH, tdg_name)
        hoffman2_tdg = os.path.join(C.HOFFMAN2_SCRATCH, tdg_name)
        print('# SCP tdg {tdg}.'.format(tdg=tdg_name))
        if scp:
            Util.call_helper([
                'scp',
                tdg,
                hoffman2_ssh_tdg
            ])
        tdg_cache = tdg + '.cache'
        tdg_cache_name = tdg_name + '.cache'
        hoffman2_ssh_tdg_cache = os.path.join(
            C.HOFFMAN2_SSH_SCRATCH, tdg_cache_name)
        print('# SCP tdg cache {tdg}.'.format(tdg=tdg_cache_name))
        if scp:
            Util.call_helper([
                'scp',
                tdg_cache,
                hoffman2_ssh_tdg_cache
            ])
        tdg_extra = tdg + '.extra'
        tdg_extra_name = tdg_name + '.extra'
        hoffman2_ssh_tdg_extra = os.path.join(
            C.HOFFMAN2_SSH_SCRATCH, tdg_extra_name)
        print('# SCP tdg extra {tdg}.'.format(tdg=tdg_extra_name))
        if scp:
            Util.call_helper([
                'scp',
                '-r',
                tdg_extra,
                hoffman2_ssh_tdg_extra
            ])
        replay_bin_name = self.get_replay_bin()
        replay_bin = os.path.join(self.get_run_path(), replay_bin_name)
        hoffman2_ssh_replay_bin = os.path.join(
            C.HOFFMAN2_SSH_SCRATCH, replay_bin_name)
        hoffman2_replay_bin = os.path.join(C.HOFFMAN2_SCRATCH, replay_bin_name)
        if self.standalone != 1:
            if scp:
                Util.call_helper([
                    'scp',
                    '-r',
                    replay_bin,
                    hoffman2_ssh_replay_bin,
                ])
        # Create the command.
        gem5_out_dir = self.gem5_config.get_config(tdg_name)
        gem5_command = self.get_gem5_simulate_command(
            hoffman2_tdg, gem5_config, hoffman2_replay_bin, gem5_out_dir, debugs, True)
        return gem5_command

    def get_hoffman2_retrive_cmd(self, tdg, gem5_config, result):
        # Create the retrive command.
        _, tdg_name = os.path.split(tdg)
        gem5_out_dir = gem5_config.get_config(tdg_name)
        retrive_cmd = [
            'scp',
            '-r',
            os.path.join(C.HOFFMAN2_SSH_SCRATCH, gem5_out_dir, '*'),
            gem5_config.get_gem5_dir(tdg),
        ]
        return retrive_cmd
