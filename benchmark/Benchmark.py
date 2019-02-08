import os
import subprocess
import glob
import abc

import Constants as C
import Util
from Utils import SimPoint


class BenchmarkArgs(object):
    def __init__(self, transform_manager, simulation_manager):
        self.transform_manager = transform_manager
        self.simulation_manager = simulation_manager


class TraceObj(object):
    def __init__(self, fn, trace_id):
        self.fn = fn
        self.trace_id = trace_id
        self.lhs = 0
        self.rhs = 0
        self.weight = 1
        self._init_simpoints_info()
        print('Find trace {weight}: {fn}'.format(
            weight=self.weight, fn=self.fn))

    def get_weight(self):
        return self.weight

    def get_trace_id(self):
        return self.trace_id

    def get_trace_fn(self):
        return self.fn

    def _init_simpoints_info(self):
        """
        Read through the simpoints file to find information for myself.
        """
        folder = os.path.dirname(self.fn)
        try:
            simpoint_fn = os.path.join(folder, 'simpoints.txt')
            if os.path.isfile(simpoint_fn):
                with open(simpoint_fn, 'r') as f:
                    trace_id = 0
                    for line in f:
                        if line.startswith('#'):
                            continue
                        if trace_id == self.trace_id:
                            # Found myself.
                            fields = line.split(' ')
                            assert(len(fields) == 3)
                            self.lhs = int(fields[0])
                            self.rhs = int(fields[1])
                            self.weight = float(fields[2])
                            break
                        trace_id += 1
        except Exception:
            return


class Benchmark(object):
    """
    The base Benchmark class does not know how to compile the program, 
    i.e. it is the derived class's responsibility to provide the llvm bc file.

    The directory tree structure.
    run_path/                               -- bc, exe, profile, traces.
    run_path/transform_id/                  -- transformed data graphs.
    run_path/transform_id/simulation_id/    -- simulation results.

    Derived classes should implement the following methods:
    get_raw_bc()


    """

    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def get_name(self):
        return

    @abc.abstractmethod
    def get_links(self):
        return

    @abc.abstractmethod
    def get_args(self):
        return

    @abc.abstractmethod
    def get_trace_func(self):
        return

    @abc.abstractmethod
    def get_lang(self):
        return

    @abc.abstractmethod
    def get_run_path(self):
        return

    @abc.abstractmethod
    def get_raw_bc(self):
        return

    def __init__(self, benchmark_args, standalone=1):

        self.transform_manager = benchmark_args.transform_manager
        self.simulation_manager = benchmark_args.simulation_manager

        # Initialize the result directory.
        Util.call_helper(
            ['mkdir', '-p', C.LLVM_TDG_RESULT_DIR])
        self.standalone = standalone

        self.pass_so = os.path.join(
            C.LLVM_TDG_BUILD_DIR, 'libLLVMTDGPass.so')

        self.trace_links = self.get_links() + [
            '-lz',
            C.PROTOBUF_LIB,
            C.LIBUNWIND_LIB,
        ]
        self.trace_lib = os.path.join(
            C.LLVM_TDG_BUILD_DIR, 'trace/libTracerProtobuf.a'
        )
        self.trace_format = 'protobuf'

        self.init_traces()

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

    def get_profile_bc(self):
        return '{name}.profiled.bc'.format(name=self.get_name())

    def get_profile_bin(self):
        return '{name}.profiled.exe'.format(name=self.get_name())

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
        return self.traces

    def init_traces(self):
        self.traces = list()
        trace_id = 0
        while True:
            trace_fn = '{run_path}/{name}.{i}.trace'.format(
                run_path=self.get_run_path(),
                name=self.get_name(),
                i=trace_id)
            if os.path.isfile(trace_fn):
                self.traces.append(TraceObj(trace_fn, trace_id))
                trace_id += 1
            else:
                break

    def get_transform_path(self, transform_id):
        return os.path.join(self.work_path, transform_id)

    def get_tdgs(self, transform_config):
        return [self.get_tdg(transform_config, trace) for trace in self.traces]

    def get_tdg(self, transform_config, trace):
        return '{transform_path}/{name}.{transform_id}.{trace_id}.tdg'.format(
            transform_path=self.get_transform_path(
                transform_config.get_transform_id()),
            name=self.get_name(),
            transform_id=transform_config.get_transform_id(),
            trace_id=trace.get_trace_id()
        )

    def init_transform_path(self, transform_id):
        transform_path = self.get_transform_path(transform_id)
        Util.mkdir_p(transform_path)

    def profile(self):
        os.chdir(self.get_run_path())
        self.build_profile()
        self.run_profile()
        os.chdir(self.cwd)

    def simpoint(self):
        os.chdir(self.work_path)
        print('Doing simpoints')
        SimPoint.SimPoint(self.get_profile())
        os.chdir(self.cwd)

    """
    Generate the profile.
    """

    def run_profile(self):
        # Remember to set the environment for profile.
        os.putenv('LLVM_TDG_WORK_MODE', '0')
        os.putenv('LLVM_TDG_MEASURE_IN_TRACE_FUNC', 'TRUE')
        os.putenv('LLVM_TDG_TRACE_FILE', self.get_name())
        run_cmd = [
            './' + self.get_profile_bin(),
        ]
        if self.get_args() is not None:
            run_cmd += self.get_args()
        print('# Run profiled binary...')
        Util.call_helper(run_cmd)

    """
    Construct the profiled binary.
    """

    def build_profile(self, link_stdlib=False, trace_reachable_only=False, debugs=[]):
        # Notice that profile does not generate inst uid.
        bc = self.get_profile_bc()
        trace_cmd = [
            C.OPT,
            '-load={PASS_SO}'.format(PASS_SO=self.pass_so),
            '-trace-pass',
            self.get_raw_bc(),
            '-o',
            bc,
            '-trace-inst-only'
        ]
        if self.get_trace_func() is not None and len(self.get_trace_func()) > 0:
            trace_cmd.append('-trace-function=' + self.get_trace_func())
        if trace_reachable_only:
            trace_cmd.append('-trace-reachable-only=1')
        if debugs:
            print(debugs)
            trace_cmd.append(
                '-debug-only={debugs}'.format(debugs=','.join(debugs)))
        print('# Instrumenting profiler...')
        Util.call_helper(trace_cmd)
        if link_stdlib:
            link_cmd = [
                C.CXX,
                '-O3',
                '-nostdlib',
                '-static',
                bc,
                self.trace_lib,
                C.MUSL_LIBC_STATIC_LIB,
                '-lc++',
                '-o',
                self.get_profile_bin(),
            ]
        else:
            link_cmd = [
                C.CXX,
                # '-O0',
                bc,
                self.trace_lib,
                '-o',
                self.get_profile_bin(),
            ]
        link_cmd += self.trace_links
        print('# Link to profiled binary...')
        Util.call_helper(link_cmd)

    """
    Generate the trace.
    """

    def run_trace(self, trace_file='llvm_trace'):
        # Remember to set the environment for trace.
        os.putenv('LLVM_TDG_TRACE_FILE', trace_file)
        run_cmd = [
            './' + self.get_trace_bin(),
        ]
        if self.get_args() is not None:
            run_cmd += self.get_args()
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
            self.get_raw_bc(),
            '-o',
            self.get_trace_bc(),
            '-trace-inst-uid-file',
            self.get_inst_uid(),
        ]
        if self.get_trace_func() is not None and len(self.get_trace_func()) > 0:
            trace_cmd.append('-trace-function=' + self.get_trace_func())
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
            self.get_raw_bc(),
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
                C.CC if self.get_lang() == 'C' else C.CXX,
                '-static',
                '-o',
                self.get_replay_bin(),
                '-I{gem5_include}'.format(gem5_include=C.GEM5_INCLUDE_DIR),
                C.GEM5_M5OPS_X86,
                C.LLVM_TDG_REPLAY_C,
                self.get_replay_bc(),
            ]
            build_cmd += self.get_links()
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
            self.get_raw_bc(),
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
            # "--debug-flags=AbstractDataFlowAccelerator",
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
            # '--l1d_mshrs={l1d_mshrs}'.format(l1d_mshrs=C.GEM5_L1D_MSHR),
            '--l1i_size={l1i_size}'.format(l1i_size=C.GEM5_L1I_SIZE),
            # '--l2_size={l2_size}'.format(l2_size=C.GEM5_L2_SIZE),
        ]

        additional_options = gem5_config.get_options(tdg)
        gem5_args += additional_options

        if debugs:
            gem5_args.insert(
                1, '--debug-flags={flags}'.format(flags=','.join(debugs)))
        if self.get_args() is not None:
            gem5_args.append(
                '--options={binary_args}'.format(binary_args=' '.join(self.get_args())))
        return gem5_args

    """
    Simulate the datagraph with gem5.
    """

    def simulate(self, tdg, simulation_config, debugs):
        print('# Simulating the datagraph')
        gem5_out_dir = simulation_config.get_gem5_dir(tdg)
        Util.call_helper(['mkdir', '-p', gem5_out_dir])
        gem5_args = self.get_gem5_simulate_command(
            tdg, simulation_config, self.get_replay_bin(), gem5_out_dir, debugs, False)
        print('# Replaying the datagraph...')
        Util.call_helper(gem5_args)

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
