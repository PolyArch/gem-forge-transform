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

    exe_path/
    This is where to instrument the bc, profiling and tracing happens.
    The traces will be moved to run_path

    run_path/                               -- bc, profile, traces.
    run_path/transform_id/                  -- transformed data graphs.
    run_path/transform_id/simulation_id/    -- simulation results.

    Derived classes should implement the following methods:
    get_name()
    get_links()
    get_args()
    get_trace_func()
    get_lang()
    get_run_path()
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
    def get_exe_path(self):
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
        self.standalone = standalone

        self.pass_so = os.path.join(
            C.LLVM_TDG_BUILD_DIR, 'libLLVMTDGPass.so')

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

    def get_trace_fn(self, trace_id):
        trace_fn = '{run_path}/{name}.{i}.trace'.format(
            run_path=self.get_run_path(),
            name=self.get_name(),
            i=trace_id)
        return trace_fn

    """
    Find the trace ids. The default implementation will search
    in the run_path. Derived class can override this behavior.
    """

    def init_trace_ids(self):
        trace_ids = list()
        trace_id = 0
        while True:
            trace_fn = self.get_trace_fn(trace_id)
            if os.path.isfile(trace_fn):
                trace_ids.append(trace_id)
                trace_id += 1
            else:
                break
        return trace_ids

    def init_traces(self):
        trace_ids = self.init_trace_ids()
        self.traces = [TraceObj(self.get_trace_fn(trace_id), trace_id)
                       for trace_id in trace_ids]

    def get_transform_path(self, transform_id):
        return os.path.join(self.get_run_path(), transform_id)

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
        os.chdir(self.get_exe_path())
        self.build_profile()
        self.run_profile()
        os.chdir(self.cwd)

    def simpoint(self):
        os.chdir(self.get_exe_path())
        print('Doing simpoints')
        SimPoint.SimPoint(self.get_profile())
        # Copy the result to run_path.
        simpont_fn = 'simpoints.txt'
        Util.call_helper([
            'cp',
            simpont_fn,
            self.get_run_path()
        ])
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
        # Clean the profile bin.
        os.remove(self.get_profile_bc())
        os.remove(self.get_profile_bin())
        # Move profile result to run_path.
        if self.get_exe_path() == self.get_run_path():
            return
        Util.call_helper([
            'cp',
            self.get_profile(),
            self.get_run_path()
        ])

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
        trace_links = self.get_links() + [
            '-lz',
            C.PROTOBUF_LIB,
            C.LIBUNWIND_LIB,
        ]
        link_cmd += trace_links
        Util.call_helper(link_cmd)

    """
    Generate the trace.
    """

    def run_trace(self, trace_file='llvm_trace'):
        # Remeber to remove all the old traces.
        Util.call_helper(
            ['rm', '-f', '{name}.*.trace'.format(name=self.get_name())])
        # Remember to set the environment for trace.
        os.putenv('LLVM_TDG_TRACE_FILE', trace_file)
        run_cmd = [
            './' + self.get_trace_bin(),
        ]
        if self.get_args() is not None:
            run_cmd += self.get_args()
        print('# Run traced binary...')
        Util.call_helper(run_cmd)
        # Clean the trace bc and bin.
        os.remove(self.get_trace_bc())
        os.remove(self.get_trace_bin())
        # Move all the traces to run_path.
        if self.get_exe_path() == self.get_run_path():
            return
        trace_id = 0
        while True:
            trace_fn = '{name}.{i}.trace'.format(
                name=self.get_name(),
                i=trace_id)
            if os.path.isfile(trace_fn):
                Util.call_helper(['mv', trace_fn, self.get_run_path()])
                trace_id += 1
            else:
                break
        Util.call_helper([
            'mv',
            '{name}.inst.uid.txt'.format(name=self.get_name()),
            self.get_run_path()
        ])

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
        trace_links = self.get_links() + [
            '-lz',
            C.PROTOBUF_LIB,
            C.LIBUNWIND_LIB,
        ]
        link_cmd += trace_links
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

    def get_additional_gem5_simulate_command(self):
        return []

    """
    Prepare the gem5 simulate command without the trace file.
    """

    def get_gem5_simulate_command(
            self,
            gem5_config,
            replay_bin,
            gem5_out_dir,
            hoffman2=False):
        gem5_args = [
            C.GEM5_X86 if not hoffman2 else C.HOFFMAN2_GEM5_X86,
            '--outdir={outdir}'.format(outdir=gem5_out_dir),
            # "--debug-flags=AbstractDataFlowAccelerator,LLVMTraceCPU",
            C.GEM5_LLVM_TRACE_SE_CONFIG if not hoffman2 else C.HOFFMAN2_GEM5_LLVM_TRACE_SE_CONFIG,
            '--cmd={cmd}'.format(cmd=replay_bin),
            '--llvm-standalone={standlone}'.format(standlone=self.standalone),
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
        ]

        additional_options = gem5_config.get_options()
        gem5_args += additional_options

        # Add any options from derived classes.
        gem5_args += self.get_additional_gem5_simulate_command()

        if self.standalone != 1:
            if self.get_args() is not None:
                gem5_args.append(
                    '--options={binary_args}'.format(binary_args=' '.join(self.get_args())))
        return gem5_args

    """
    Simulate a single datagraph with gem5.
    """

    def simulate(self, tdg, simulation_config):
        print('# Simulating the datagraph')
        gem5_out_dir = simulation_config.get_gem5_dir(tdg)
        Util.call_helper(['mkdir', '-p', gem5_out_dir])
        gem5_args = self.get_gem5_simulate_command(
            simulation_config, self.get_replay_bin(), gem5_out_dir, False)
        # Remember to add back the trace file options.
        gem5_args.append(
            '--llvm-trace-file={trace_file}'.format(trace_file=tdg)
        )
        print('# Replaying the datagraph...')
        Util.call_helper(gem5_args)
