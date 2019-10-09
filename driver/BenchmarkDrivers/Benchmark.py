import os
import subprocess
import glob
import abc

import Constants as C
import Util
from Utils import SimPoint
from Utils import Gem5McPAT
from Utils import TraceFlagEnum


class BenchmarkArgs(object):
    def __init__(self, transform_manager, simulation_manager, options):
        self.transform_manager = transform_manager
        self.simulation_manager = simulation_manager
        self.options = options


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

    def __init__(self, benchmark_args, standalone=True):

        self.transform_manager = benchmark_args.transform_manager
        self.simulation_manager = benchmark_args.simulation_manager
        self.options = benchmark_args.options

        self.standalone = standalone

        self.pass_so = os.path.join(
            C.LLVM_TDG_BUILD_DIR, 'src', 'libLLVMTDGPass.so')

        self.trace_lib = os.path.join(
            C.LLVM_TDG_BUILD_DIR, 'src', 'trace/libTracerProtobuf.a'
        )
        self.trace_format = 'protobuf'

        self.init_traces()

    def debug(self, msg):
        print('> {name} <: {msg}'.format(name=self.get_name(), msg=msg))

    def get_name(self):
        assert('get_name is not implemented by derived class')

    def get_perf_frequency(self):
        return 100

    def get_unmodified_bin(self):
        return '{name}.exe'.format(name=self.get_name())

    def get_profile_bc(self):
        return '{name}.profiled.bc'.format(name=self.get_name())

    def get_profile_bin(self):
        return '{name}.profiled.exe'.format(name=self.get_name())

    def get_profile_roi(self):
        return TraceFlagEnum.GemForgeTraceROI.All.value

    def get_trace_bc(self):
        return '{name}.traced.bc'.format(name=self.get_name())

    def get_profile_inst_uid(self):
        return '{name}.profile.inst.uid.txt'.format(name=self.get_name())

    def get_inst_uid(self):
        return '{name}.inst.uid'.format(name=self.get_name())

    def get_inst_uid_txt(self):
        return '{name}.inst.uid.txt'.format(name=self.get_name())

    def get_trace_bin(self):
        return '{name}.traced.exe'.format(name=self.get_name())

    def get_replay_bc(self):
        return '{name}.replay.bc'.format(name=self.get_name())

    def get_replay_bin(self):
        return '{name}.replay.exe'.format(name=self.get_name())

    def get_valid_bin(self):
        return '{name}.valid.exe'.format(name=self.get_name())

    def get_profile(self):
        # This only works for single thread workloads (0 -> main thread).
        # TODO: Search for profile for non-main thread.
        return '{name}.pf.0.profile'.format(name=self.get_name())

    def get_traces(self):
        return self.traces

    def get_hard_exit_in_billion(self):
        """
        Get the number of billion instructions for hard exit limit
        for both profile and simulation.
        """
        return 10

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
        """
        Originally for single thread workloads, the trace id will
        be encoded in the trace file name. However, after we enable
        tracing for multi-threaded workloads, the trace file name 
        will be {benchmark_name}.{thread_id}.{trace_id}.trace, with
        the main thread always be assigned to 0.
        We find all traces and sort them to have a consistant scalar
        trace id assigned to all traces.
        """
        trace_fns = glob.glob(os.path.join(
            self.get_run_path(),
            '{name}.*.trace'.format(name=self.get_name())
        ))
        # Sort them.
        trace_fns.sort()
        self.traces = list()
        for trace_id in range(len(trace_fns)):
            trace_fn = trace_fns[trace_id]
            print(trace_fn, trace_id)
            # Ignore the trace id not specified by the user.
            if self.options.trace_id:
                if trace_id not in self.options.trace_id:
                    continue
            self.traces.append(
                TraceObj(trace_fn, trace_id)
            )

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

    def get_tdg_extra_path(self, transform_config, trace):
        return self.get_tdg(transform_config, trace) + '.extra'

    def init_transform_path(self, transform_id):
        transform_path = self.get_transform_path(transform_id)
        Util.mkdir_p(transform_path)

    def profile(self):
        os.chdir(self.get_exe_path())
        # Copy bc from workpath.
        if self.get_run_path() != self.get_exe_path():
            Util.call_helper([
                'cp',
                '-f',
                os.path.join(self.get_run_path(), self.get_raw_bc()),
                '.',
            ])
        self.build_profile()
        self.run_profile()
        os.chdir(self.cwd)

    def simpoint(self):
        os.chdir(self.get_exe_path())
        print('Doing simpoints')
        SimPoint.SimPoint(self.get_profile())
        # Copy the result to run_path.
        if self.get_exe_path() != self.get_run_path():
            simpont_fn = 'simpoints.txt'
            Util.call_helper([
                'cp',
                simpont_fn,
                self.get_run_path()
            ])
        os.chdir(self.cwd)

    def perf(self):
        os.chdir(self.get_exe_path())
        self.build_unmodified()
        self.run_perf()
        os.chdir(self.cwd)

    """
    Constructed the unmodified binary.
    """

    def build_unmodified(self):
        build_cmd = [
            C.CC if self.get_lang() == 'C' else C.CXX,
            self.get_raw_bc(),
            '-o',
            self.get_unmodified_bin()
        ]
        build_cmd += self.get_links()
        Util.call_helper(build_cmd)

    """
    Run the unmodified binary with perf.
    """

    def run_perf(self):
        Util.call_helper([
            'rm',
            '-f',
            'perf.data',
        ])
        perf_cmd = [
            'perf',
            'record',
            '-m',
            '4',
            '-F',
            str(self.get_perf_frequency()),
            '-e',
            'instructions',
            './' + self.get_unmodified_bin(),
        ]
        if self.get_args() is not None:
            perf_cmd += self.get_args()
        Util.call_helper(perf_cmd)
        # So far let's just dump the perf to file.
        with open('perf.txt', 'w') as f:
            Util.call_helper(['perf', 'report'], stdout=f)
        # Move perf result to run_path.
        if self.get_exe_path() == self.get_run_path():
            return
        Util.call_helper([
            'mv',
            'perf.txt',
            self.get_run_path()
        ])

    """
    Run a specified opt analysis.
    """

    def opt_analyze(self):
        os.chdir(self.get_run_path())
        opt_cmd = [
            C.OPT,
            '-{opt_analyze}'.format(opt_analyze=self.options.opt_analyze),
            self.get_raw_bc(),
            '-analyze'
        ]
        with open('{opt_analyze}.txt'.format(opt_analyze=self.options.opt_analyze), 'w') as f:
            Util.call_helper(opt_cmd, stdout=f)
        os.chdir(self.cwd)

    """
    Construct the profiled binary.
    """

    def build_profile(self, link_stdlib=False, trace_reachable_only=False):
        # Notice that profile does not generate inst uid.
        bc = self.get_profile_bc()
        trace_cmd = [
            C.OPT,
            '-load={PASS_SO}'.format(PASS_SO=self.pass_so),
            '-trace-pass',
            self.get_raw_bc(),
            '-o',
            bc,
            '-trace-inst-only',
            '-trace-inst-uid-file',
            self.get_profile_inst_uid(),
        ]
        if self.get_trace_func() is not None and len(self.get_trace_func()) > 0:
            trace_cmd.append('-trace-function=' + self.get_trace_func())
        if trace_reachable_only:
            trace_cmd.append('-trace-reachable-only=1')
        if self.options.transform_debug:
            trace_cmd.append(
                '-debug-only={debugs}'.format(debugs=self.options.transform_debug))
        print('# Instrumenting profiler...')
        Util.call_helper(trace_cmd)
        if link_stdlib:
            link_cmd = C.get_cxx_cmd(C.CXX)
            link_cmd += [
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
            link_cmd = C.get_cxx_cmd(C.CXX)
            link_cmd += [
                bc,
                self.trace_lib,
                '-o',
                self.get_profile_bin(),
            ]
        trace_links = self.get_links() + [
            '-I{gem5_include}'.format(gem5_include=C.GEM5_INCLUDE_DIR),
            C.GEM5_M5OPS_EMPTY,
            '-lz',
            '-pthread',
            C.PROTOBUF_LIB,
            C.LIBUNWIND_LIB,
        ]
        link_cmd += trace_links
        Util.call_helper(link_cmd)
    """
    Generate the profile.
    """

    def run_profile(self):
        # Remember to set the environment for profile.
        # By default it will profile all dynamic instructions.
        # Derived class can set LLVM_TDG_TRACE_ROI to override this behavior.
        os.putenv('LLVM_TDG_TRACE_MODE', str(
            TraceFlagEnum.GemForgeTraceMode.Profile.value
        ))
        os.putenv('LLVM_TDG_TRACE_ROI', str(self.get_profile_roi()))
        os.putenv('LLVM_TDG_TRACE_FILE', self.get_name() + '.pf')
        os.putenv('LLVM_TDG_INST_UID_FILE', self.get_profile_inst_uid())
        os.putenv('LLVM_TDG_HARD_EXIT_IN_BILLION',
                  str(self.get_hard_exit_in_billion()))
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
    Generate the trace.
    """

    def run_trace(self, trace_file='llvm_trace'):
        # Remeber to remove all the old traces.
        Util.call_helper(
            ['rm', '-f', '{name}.*.trace'.format(name=self.get_name())])
        # Remember to set the environment for trace.
        os.putenv('LLVM_TDG_TRACE_FILE', trace_file)
        os.putenv('LLVM_TDG_INST_UID_FILE', self.get_inst_uid())
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
        trace_fns = glob.glob('{name}.*.trace'.format(
            name=self.get_name(),
        ))
        for trace_fn in trace_fns:
            Util.call_helper(['mv', trace_fn, self.get_run_path()])
        Util.call_helper([
            'mv',
            self.get_inst_uid(),
            self.get_run_path()
        ])
        Util.call_helper([
            'mv',
            self.get_inst_uid_txt(),
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
        if self.options.transform_debug:
            trace_cmd.append(
                '-debug-only={debugs}'.format(debugs=self.options.transform_debug))
        print('# Instrumenting tracer...')
        Util.call_helper(trace_cmd)
        if link_stdlib:
            link_cmd = C.get_cxx_cmd(C.CXX)
            link_cmd += [
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
            link_cmd = C.get_cxx_cmd(C.CXX)
            link_cmd += [
		'-v',
                self.get_trace_bc(),
                self.trace_lib,
                '-o',
                self.get_trace_bin(),
            ]
        trace_links = self.get_links() + [
            '-I{gem5_include}'.format(gem5_include=C.GEM5_INCLUDE_DIR),
            C.GEM5_M5OPS_EMPTY,
            '-lz',
            '-pthread',
            C.PROTOBUF_LIB,
            C.LIBUNWIND_LIB,
        ]
        link_cmd += trace_links
        print('# Link to traced binary...')
        Util.call_helper(link_cmd)

    """
    Abstract function to build the validation binary.
    """

    def build_validation(self, transform_config, trace, output_tdg):
        raise NotImplementedError

    def get_additional_transform_options(self):
        return list()

    """
    Construct the replay binary from the trace.
    """

    def build_replay(self,
                     transform_config,
                     trace,
                     tdg_detail='integrated',
                     output_tdg=None,
                     ):
        # Special case for validation, which is a raw binary.
        if transform_config.get_transform_id() == 'valid':
            self.build_validation(transform_config, trace, output_tdg)
            return
        opt_cmd = [
            C.OPT,
            '-load={PASS_SO}'.format(PASS_SO=self.pass_so),
        ]
        transform_options = transform_config.get_options(self, trace)
        opt_cmd += transform_options
        opt_cmd += [
            '-trace-file={trace_file}'.format(trace_file=trace.get_trace_fn()),
            '-datagraph-inst-uid-file={inst_uid}'.format(
                inst_uid=self.get_inst_uid()),
            '-tdg-profile-file={profile_file}'.format(
                profile_file=self.get_profile()),
            '-trace-format={format}'.format(format=self.trace_format),
            '-datagraph-detail={detail}'.format(detail=tdg_detail),
            self.get_raw_bc(),
            '-o',
            self.get_replay_bc(),
        ]
        if self.options.transform_text:
            opt_cmd.append('-output-datagraph-text-mode=true')
        # Add the additional options.
        opt_cmd += self.get_additional_transform_options()
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
        if self.options.transform_debug:
            opt_cmd.append(
                '-debug-only={debugs}'.format(debugs=self.options.transform_debug))
        if self.options.perf_command:
            opt_cmd = ['perf', 'record'] + opt_cmd
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

    def get_replay_exe(self, transform_config, trace, suffix):
        return os.path.join(
            self.get_tdg_extra_path(transform_config, trace),
            # So far the bc is named after transform_id.bc
            '{tid}.{suffix}'.format(
                tid=transform_config.get_transform_id(),
                suffix=suffix,
            ),
        )

    def build_replay_exe(self, transform_config, trace):
        transformed_bc = self.get_replay_exe(transform_config, trace, 'bc')
        transformed_obj = self.get_replay_exe(transform_config, trace, 'o')
        compile_cmd = [
            # Use DEBUG compiler?
            C.CC_DEBUG if self.get_lang() == 'C' else C.CXX_DEBUG,
            '-c',
            '-O3',
            '--target=riscv64-unknown-linux-gnu',
            '-march=rv64g',
            '-mabi=lp64d',
            transformed_bc,
            '-o',
            transformed_obj,
        ]
        Util.call_helper(compile_cmd)
        if self.options.transform_text:
            # Disassembly it for debug purpose.
            transformed_asm = self.get_replay_exe(transform_config, trace, 's')
            disasm_cmd = [
                # Use DEBUG compiler?
                C.CC_DEBUG if self.get_lang() == 'C' else C.CXX_DEBUG,
                '-c',
                '-S',
                '-O3',
                '--target=riscv64-unknown-linux-gnu',
                '-march=rv64g',
                '-mabi=lp64d',
                transformed_bc,
                '-o',
                transformed_asm,
            ]
            Util.call_helper(disasm_cmd)
            # ! Don't use llvm_objdump as it does not decode floating instructions.
            # with open(transformed_asm, 'w') as asm:
            #     disasm_cmd = [
            #         C.LLVM_OBJDUMP_DEBUG,
            #         '-d',
            #         '-march=rv64g',
            #         transformed_obj,
            #     ]
            #     Util.call_helper(disasm_cmd, stdout=asm)
        # Link them into code.
        transformed_exe = self.get_replay_exe(transform_config, trace, 'exe')
        link_cmd = [
            os.path.join(C.RISCV_GNU_INSTALL_PATH,
                         'bin/riscv64-unknown-linux-gnu-g++'),
            '-static',
            '-march=rv64g',
            '-mabi=lp64d',
            '-o',
            transformed_exe,
            transformed_obj,
        ]
        link_cmd += self.get_links()
        link_cmd += [
            '-I{gem5_include}'.format(gem5_include=C.GEM5_INCLUDE_DIR),
            C.GEM5_M5OPS_RISCV,
        ]
        Util.call_helper(link_cmd)

    def transform(self, transform_config, trace, tdg):
        cwd = os.getcwd()
        os.chdir(self.get_run_path())

        self.build_replay(
            transform_config=transform_config,
            trace=trace,
            tdg_detail='standalone',
            output_tdg=tdg,
        )

        if transform_config.is_execution_transform():
            self.build_replay_exe(transform_config, trace)

        os.chdir(cwd)

    def get_additional_gem5_simulate_command(self):
        return []

    """
    Prepare the gem5 simulate command without the trace file.
    """

    def get_gem5_simulate_command(
            self,
            simulation_config,
            binary,
            outdir,
            standalone):
        hoffman2 = False
        gem5_args = [
            # C.GEM5_X86 if not hoffman2 else C.HOFFMAN2_GEM5_X86,
            C.GEM5_RISCV,
            '--outdir={outdir}'.format(outdir=outdir),
            C.GEM5_LLVM_TRACE_SE_CONFIG if not hoffman2 else C.HOFFMAN2_GEM5_LLVM_TRACE_SE_CONFIG,
            '--cmd={cmd}'.format(cmd=binary),
            '--llvm-issue-width={ISSUE_WIDTH}'.format(
                ISSUE_WIDTH=C.ISSUE_WIDTH),
            '--llvm-store-queue-size={STORE_QUEUE_SIZE}'.format(
                STORE_QUEUE_SIZE=C.STORE_QUEUE_SIZE),
            '--llvm-mcpat={use_mcpat}'.format(use_mcpat=C.GEM5_USE_MCPAT),
            '--caches',
            '--l2cache',
            '--cpu-type={cpu_type}'.format(cpu_type=C.CPU_TYPE),
        ]
        if self.options.gem5_debug is not None:
            gem5_args.insert(
                1, '--debug-flags={debug}'.format(debug=self.options.gem5_debug))
        if self.options.gem5_max_insts is not None:
            gem5_args.append(
                '--maxinsts={max_insts}'.format(max_insts=self.options.gem5_max_insts))

        if standalone:
            gem5_args.append('--llvm-standalone')

        additional_options = simulation_config.get_options()
        gem5_args += additional_options

        # Add any options from derived classes.
        gem5_args += self.get_additional_gem5_simulate_command()

        # Append the arguments.
        if self.get_args() is not None:
            gem5_args.append(
                '--options={binary_args}'.format(binary_args=' '.join(self.get_args())))
        return gem5_args

    """
    Abstract function to simulate validation.
    """

    def simulate_valid(self, tdg, transform_config, simulation_config):
        raise NotImplementedError

    def simulate_execution_transform(self, trace, transform_config, simulation_config):
        assert(transform_config.is_execution_transform())
        tdg = self.get_tdg(transform_config, trace)
        gem5_out_dir = simulation_config.get_gem5_dir(tdg)
        gem5_args = self.get_gem5_simulate_command(
            simulation_config=simulation_config,
            binary=self.get_replay_exe(transform_config, trace, 'exe'),
            outdir=gem5_out_dir,
            standalone=False,
        )
        # Do not add the tdg file, so that gem5 will simulate the binary.
        # For execution simulation, we would like to be in the exe_path.
        cwd = os.getcwd()
        os.chdir(self.get_exe_path())
        Util.call_helper(gem5_args)
        os.chdir(cwd)

    """
    Simulate a single datagraph with gem5.
    """

    def simulate(self, trace, transform_config, simulation_config):
        if transform_config.get_transform_id() == 'valid':
            self.simulate_valid(tdg, transform_config, simulation_config)
            return
        if transform_config.is_execution_transform():
            self.simulate_execution_transform(
                trace, transform_config, simulation_config)
            return

        print('# Simulating the datagraph')
        tdg = self.get_tdg(transform_config, trace)
        gem5_out_dir = simulation_config.get_gem5_dir(tdg)
        Util.call_helper(['mkdir', '-p', gem5_out_dir])
        gem5_args = self.get_gem5_simulate_command(
            simulation_config=simulation_config,
            binary=self.get_replay_bin(),
            outdir=gem5_out_dir,
            standalone=self.standalone
        )
        # Remember to add back the trace file options.
        gem5_args.append(
            '--llvm-trace-file={trace_file}'.format(trace_file=tdg)
        )
        if self.options.perf_command:
            gem5_args = ['perf', 'record'] + gem5_args
        Util.call_helper(gem5_args)

    """
    Run McPAT for simulated results.
    """

    def mcpat(self, tdg, transform_config, simulation_config):
        if transform_config.get_transform_id() == 'valid':
            assert(False)
        gem5_out_dir = simulation_config.get_gem5_dir(tdg)
        gem5_mcpat = Gem5McPAT.Gem5McPAT(gem5_out_dir)

    """
    Clean the results.
    """

    def clean(self, target):
        for trace in self.get_traces():
            if target == 'transform':
                for transform_config in self.transform_manager.get_all_configs():
                    tdg = self.get_tdg(transform_config, trace)
                    print('Clean {tdg}.'.format(tdg=tdg))
                    Util.call_helper(['rm', '-f', tdg])
                    Util.call_helper(['rm', '-f', tdg + '.cache'])
                    Util.call_helper(['rm', '-f', tdg + '.stats.txt'])
                    Util.call_helper(['rm', '-rf', tdg + '.extra'])
            if target == 'trace':
                print('Clean {trace}.'.format(trace=trace.fn))
                Util.call_helper(['rm', '-f', trace.fn])
