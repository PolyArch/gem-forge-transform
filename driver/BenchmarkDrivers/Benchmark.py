import os
import subprocess
import glob
import abc

import Constants as C
import Util
from Utils import SimPoint
from Utils import TraceFlagEnum
import Utils.Gem5McPAT.Gem5McPAT as Gem5McPAT


class BenchmarkArgs(object):
    def __init__(self, transform_manager, simulation_manager, options):
        self.transform_manager = transform_manager
        self.simulation_manager = simulation_manager
        self.options = options


class TraceObj(object):
    def __init__(self, fn, trace_id, weight):
        self.fn = fn
        self.trace_id = trace_id
        self.lhs = 0
        self.rhs = 0
        self.weight = weight
        print('Find trace {weight}: {fn}'.format(
            weight=self.weight, fn=self.fn))

    def get_weight(self):
        return self.weight

    def get_trace_id(self):
        return self.trace_id

    def get_trace_fn(self):
        return self.fn


class Benchmark(object):
    """
    The base Benchmark class does not know how to compile the program, 
    i.e. it is the derived class's responsibility to provide the llvm bc file.

    The directory tree structure.

    exe_path/
    This is where to instrument the bc, profiling and tracing happens.
    The traces will be moved to run_path

    run_path/                               -- bc, profile, traces.
    run_path/profile/                       -- profile results.
    run_path/trace/                         -- trace results.
    run_path/transform_id/                  -- transformed data graphs.
    run_path/transform_id/simulation_id/    -- simulation results.

    Derived classes should implement the following methods:
    get_name()
    get_links()
    get_args()
    get_trace_func()
    get_lang()
    get_run_path()

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

    def get_sim_exe_path(self):
        return self.get_exe_path()

    @abc.abstractmethod
    def get_run_path(self):
        return

    def get_gem5_links(self):
        return self.get_links()

    def get_gem5_mem_size(self):
        return None

    """
    Used to separate multiple functions.
    """
    ROI_FUNC_SEPARATOR = '|'

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

    def get_input_size(self):
        # Please override if the suite has various input_size.
        return None

    def get_sim_input_size(self):
        # Please override if the suite has various input_size.
        return None

    def get_perf_frequency(self):
        return 100

    def get_raw_bc(self):
        return 'raw.bc'

    def get_unmodified_bin(self):
        return '{name}.exe'.format(name=self.get_name())

    def get_replay_bc(self):
        return '{name}.replay.bc'.format(name=self.get_name())

    def get_replay_bin(self):
        return '{name}.replay.exe'.format(name=self.get_name())

    def get_valid_bin(self):
        return '{name}.valid.exe'.format(name=self.get_name())

    """
    Get some constant values for profile.
    """

    def get_profile_base(self):
        ret = 'profile'
        if self.get_input_size():
            ret += '.' + self.get_input_size()
        return ret

    def get_profile_folder_abs(self):
        return os.path.join(self.get_run_path(), self.get_profile_base())

    def get_profile_bc(self):
        return '{b}.bc'.format(b=self.get_profile_base())

    def get_profile_bin(self):
        return '{b}.exe'.format(b=self.get_profile_base())

    def get_profile_inst_uid(self):
        return os.path.join(self.get_profile_folder_abs(), 'inst.uid')

    def get_profile_roi(self):
        return TraceFlagEnum.GemForgeTraceROI.All.value

    def get_profile(self):
        # This only works for single thread workloads (0 -> main thread).
        # TODO: Search for profile for non-main thread.
        return os.path.join(self.get_profile_folder_abs(), '0.profile')

    """
    Get some constant values for simpoint.
    """

    def get_simpoint_abs(self):
        return os.path.join(self.get_profile_folder_abs(), 'simpoints.txt')

    def get_region_simpoint_abs(self):
        return os.path.join(self.get_profile_folder_abs(), 'region.simpoints.txt')

    """
    Get some constant values for trace.
    """

    def get_trace_base(self):
        ret = 'trace'
        if self.get_input_size():
            ret += '.' + self.get_input_size()
        return ret

    def get_trace_folder_abs(self):
        return os.path.join(self.get_run_path(), self.get_trace_base())

    def get_trace_bc(self):
        return '{b}.bc'.format(b=self.get_trace_base())

    def get_trace_bin(self):
        return '{b}.exe'.format(b=self.get_trace_base())

    def get_trace_inst_uid(self):
        return os.path.join(self.get_trace_folder_abs(), 'inst.uid')

    def get_trace_inst_uid_txt(self):
        return self.get_trace_inst_uid() + '.txt'

    def get_traces(self):
        return self.traces

    def get_sim_args(self):
        """
        As an extension, we support execution simulation with different
        input than the trace. Default to get_args().
        """
        return self.get_args()

    def get_hard_exit_in_billion(self):
        """
        Get the number of billion instructions for hard exit limit
        for both profile and simulation.
        """
        return 10

    def init_traces(self):
        # First try to init from simpoint.
        self.traces = list()
        if self.options.region_simpoint and os.path.isfile(self.get_region_simpoint_abs()):
            self.init_traces_from_region_simpoint(
                self.get_region_simpoint_abs())
            return
        if os.path.isfile(self.get_simpoint_abs()):
            self.init_traces_from_simpoint(self.get_simpoint_abs())
            return
        self.init_traces_from_glob()

    def init_traces_from_simpoint(self, simpoint_fn):
        """
        Read in the simpoint and try to find the trace.
        Since simpoint only works for single thread workloads,
        we will always assume thread id to be zero.
        """
        with open(simpoint_fn, 'r') as f:
            trace_id = 0
            for line in f:
                if line.startswith('#'):
                    continue
                fields = line.split(' ')
                assert(len(fields) == 3)
                lhs = int(fields[0])
                rhs = int(fields[1])
                weight = float(fields[2])
                trace_fn = os.path.join(
                    self.get_trace_folder_abs(),
                    '0.{tid}.trace'.format(tid=trace_id),
                )
                assert(os.path.isfile(trace_fn))
                self.traces.append(
                    TraceObj(trace_fn, trace_id, weight)
                )
                trace_id += 1

    def init_traces_fake(self):
        trace_fn = os.path.join(
            self.get_trace_folder_abs(),
            'fake.trace',
        )
        trace_id = 0
        weight = 1.0
        self.traces.append(
            TraceObj(trace_fn, trace_id, weight)
        )

    def init_traces_from_region_simpoint(self, region_simpoint_fn):
        Util.mkdir_p(self.get_trace_folder_abs())
        with open(region_simpoint_fn, 'r') as f:
            trace_id = 0
            for line in f:
                if line.startswith('#'):
                    continue
                fields = line.split()
                print(fields)
                assert(len(fields) == 6)
                weight = float(fields[0])
                func = fields[4]
                bb = fields[5]
                # Write the fake trace file.
                trace_fn = os.path.join(
                    self.get_trace_folder_abs(),
                    'region.{tid}.trace'.format(tid=trace_id)
                )
                with open(trace_fn, 'w') as trace:
                    trace.write(func + '\n')
                    trace.write(bb + '\n')
                self.traces.append(
                    TraceObj(trace_fn, trace_id, weight)
                )
                trace_id += 1

    def init_traces_from_glob(self):
        """
        Originally for single thread workloads, the trace id will
        be encoded in the trace file name. However, after we enable
        tracing for multi-threaded workloads, the trace file name 
        will be {thread_id}.{trace_id}.trace, with
        the main thread always be assigned to 0.
        We find all traces and sort them to have a consistant scalar
        trace id assigned to all traces.
        ! Notice that we can't sort directly, which will mess the 
        ! relationship between the trace and simpoint weight for
        ! single-thread workload.
        """
        trace_fns = glob.glob(os.path.join(
            self.get_trace_folder_abs(),
            '*.trace',
        ))
        # Filter out those region traces.
        trace_fns = [t for t in trace_fns if not os.path.basename(
            t).startswith('region.')]
        print(trace_fns)
        # Sort them.

        def sort_by(a, b):
            a_fields = os.path.basename(a).split('.')
            b_fields = os.path.basename(b).split('.')
            a_thread_id = int(a_fields[-3])
            b_thread_id = int(b_fields[-3])
            a_trace_id = int(a_fields[-2])
            b_trace_id = int(b_fields[-2])
            if a_thread_id == b_thread_id:
                return a_trace_id - b_trace_id
            else:
                return a_thread_id - b_thread_id
        trace_fns.sort(cmp=sort_by)
        for trace_id in range(len(trace_fns)):
            trace_fn = trace_fns[trace_id]
            # Ignore the trace id not specified by the user.
            if self.options.trace_id:
                if trace_id not in self.options.trace_id:
                    continue
            self.traces.append(
                TraceObj(trace_fn, trace_id, 1.0)
            )

    def get_transform_path(self, transform_id):
        return os.path.join(self.get_run_path(), transform_id)

    def get_tdgs(self, transform_config):
        return [self.get_tdg(transform_config, trace) for trace in self.traces]

    def get_tdg(self, transform_config, trace):
        if self.get_input_size():
            return '{transform_path}/{input_size}.{trace_id}.tdg'.format(
                transform_path=self.get_transform_path(
                    transform_config.get_transform_id()),
                input_size=self.get_input_size(),
                trace_id=trace.get_trace_id(),
            )
        else:
            return '{transform_path}/{trace_id}.tdg'.format(
                transform_path=self.get_transform_path(
                    transform_config.get_transform_id()),
                trace_id=trace.get_trace_id(),
            )

    def get_tdg_extra_path(self, transform_config, trace):
        return self.get_tdg(transform_config, trace) + '.extra'

    def init_transform_path(self, transform_id):
        transform_path = self.get_transform_path(transform_id)
        Util.mkdir_p(transform_path)

    def simpoint(self):
        os.chdir(self.get_exe_path())
        print('Doing simpoints')
        SimPoint.SimPoint(self.get_profile(), self.get_simpoint_abs())
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

    def profile(self):
        os.chdir(self.get_exe_path())
        # Remove the existing profile.
        profile_folder = self.get_profile_folder_abs()
        Util.mkdir_f(profile_folder)
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
        # self.analyze_profile()
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
            link_cmd = C.get_native_cxx_compiler(C.CXX)
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
            link_cmd = C.get_native_cxx_compiler(C.CXX)
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
        os.putenv('LLVM_TDG_TRACE_FOLDER', self.get_profile_folder_abs())
        os.putenv('LLVM_TDG_INST_UID_FILE', self.get_profile_inst_uid())
        os.putenv('LLVM_TDG_HARD_EXIT_IN_MILLION',
                  str(self.get_hard_exit_in_billion() * 1000))
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

    """
    Analyze the profile.
    """

    def analyze_profile(self):
        analyze_cmd = [
            C.OPT,
            '-load={PASS_SO}'.format(PASS_SO=self.pass_so),
            '-profile-analyze-pass',
            self.get_raw_bc(),
            '-gem-forge-inst-uid-file',
            self.get_profile_inst_uid(),
            '-gem-forge-profile-folder',
            self.get_profile_folder_abs(),
        ]
        Util.call_helper(analyze_cmd)

    """
    Generate the trace.
    """

    def run_trace(self):
        if self.options.fake_trace:
            # Do not bother really run the trace
            # but generate a fake one.
            trace_fn = os.path.join(
                self.get_trace_folder_abs(),
                'fake.0.0.trace',
            )
            if os.path.isfile(trace_fn):
                rm_cmd = ['rm', trace_fn]
                Util.call_helper(trace_fn)
            touch_cmd = ['touch', trace_fn]
            Util.call_helper(touch_cmd)
        else:
            # Remember to set the environment for trace.
            os.putenv('LLVM_TDG_TRACE_FOLDER', self.get_trace_folder_abs())
            os.putenv('LLVM_TDG_INST_UID_FILE', self.get_trace_inst_uid())
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

    """
    Construct the traced binary.
    """

    def build_trace(self, link_stdlib=False, trace_reachable_only=False, debugs=[]):
        # Remeber to clear the trace folder.
        trace_folder = self.get_trace_folder_abs()
        Util.mkdir_f(trace_folder)
        trace_cmd = [
            C.OPT,
            '-load={PASS_SO}'.format(PASS_SO=self.pass_so),
            '-trace-pass',
            self.get_raw_bc(),
            '-o',
            self.get_trace_bc(),
            '-trace-inst-uid-file',
            self.get_trace_inst_uid(),
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
            link_cmd = C.get_native_cxx_compiler(C.CXX)
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
            link_cmd = C.get_native_cxx_compiler(C.CXX)
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
            '-gem-forge-profile-folder={profile_folder}'.format(
                profile_folder=self.get_profile_folder_abs()),
            '-trace-format={format}'.format(format=self.trace_format),
            '-datagraph-detail={detail}'.format(detail=tdg_detail),
            self.get_raw_bc(),
            '-o',
            self.get_replay_bc(),
        ]
        if self.options.transform_text:
            opt_cmd.append('-output-datagraph-text-mode=true')
        if self.get_trace_func():
            opt_cmd.append(
                '-gem-forge-roi-function={f}'.format(f=self.get_trace_func()))
        if self.options.region_simpoint:
            opt_cmd.append('-gem-forge-region-simpoint=true')
            # Region simpoint requires profile inst uid.
            opt_cmd.append(
                '-gem-forge-inst-uid-file={inst_uid}'.format(
                    inst_uid=self.get_profile_inst_uid()),
            )
        else:
            opt_cmd.append(
                '-gem-forge-inst-uid-file={inst_uid}'.format(
                    inst_uid=self.get_trace_inst_uid()),
            )
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
            # So far the bc is named after ex.bc
            'ex.{suffix}'.format(
                suffix=suffix,
            ),
        )

    def build_replay_exe(self, transform_config, trace):
        transformed_bc = self.get_replay_exe(transform_config, trace, 'bc')
        transformed_obj = self.get_replay_exe(transform_config, trace, 'o')
        compiler = C.CC_DEBUG if self.get_lang() == 'C' else C.CXX_DEBUG
        compile_cmd = C.get_sim_compiler(compiler) + [
            '-c',
            '-O3',
            transformed_bc,
            '-o',
            transformed_obj,
        ]
        Util.call_helper(compile_cmd)
        if self.options.transform_text:
            # Disassembly it for debug purpose.
            transformed_asm = self.get_replay_exe(transform_config, trace, 's')
            disasm_cmd = C.get_sim_compiler(compiler) + [
                '-c',
                '-S',
                '-O3',
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
        link_cmd = C.get_sim_linker() + [
            '-static',
            '-o',
            transformed_exe,
            transformed_obj,
        ]
        link_cmd += self.get_gem5_links()
        link_cmd += [
            '-I{gem5_include}'.format(gem5_include=C.GEM5_INCLUDE_DIR),
            C.get_gem5_m5ops(),
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
            C.get_gem5(),
            '--outdir={outdir}'.format(outdir=outdir),
            # Always dump all stats.
            '--stats-file=text://stats.txt?dumpAll=False',
            '--listener-mode=off',
            C.GEM5_LLVM_TRACE_SE_CONFIG if not hoffman2 else C.HOFFMAN2_GEM5_LLVM_TRACE_SE_CONFIG,
            '--cmd={cmd}'.format(cmd=binary),
            '--llvm-store-queue-size={STORE_QUEUE_SIZE}'.format(
                STORE_QUEUE_SIZE=C.STORE_QUEUE_SIZE),
            '--llvm-mcpat={use_mcpat}'.format(use_mcpat=C.GEM5_USE_MCPAT),
            '--caches',
            '--l2cache',
        ]
        if self.options.gem5_debug is not None:
            gem5_args.insert(
                1, '--debug-flags={debug}'.format(debug=self.options.gem5_debug))
        if self.options.gem5_debug_start is not None:
            gem5_args.insert(
                1, '--debug-start={d}'.format(d=self.options.gem5_debug_start))
        if self.options.gem5_max_insts is not None:
            gem5_args.append(
                '--maxinsts={max_insts}'.format(max_insts=self.options.gem5_max_insts))

        if standalone:
            gem5_args.append('--llvm-standalone')

        additional_options = simulation_config.get_options()
        gem5_args += additional_options

        # Add any options from derived classes.
        gem5_args += self.get_additional_gem5_simulate_command()

        # Allow each benchmark to have a customized memory capacity.
        if self.get_gem5_mem_size():
            mem_size = self.get_gem5_mem_size()
            print('Reset MemSize = {s}'.format(s=mem_size))
            reset = False
            for idx in range(len(gem5_args)):
                if gem5_args[idx].startswith('--mem-size='):
                    gem5_args[idx] = '--mem-size={s}'.format(s=mem_size)
                    reset = True
                    break
            if not reset:
                gem5_args.append('--mem-size={s}'.format(s=mem_size))

        # Append the arguments.
        if self.get_sim_args() is not None:
            gem5_args.append(
                '--options={binary_args}'.format(binary_args=' '.join(self.get_sim_args())))
        return gem5_args

    """
    Abstract function to simulate validation.
    """

    def simulate_valid(self, tdg, transform_config, simulation_config):
        raise NotImplementedError

    def simulate_execution_transform(self, trace, transform_config, simulation_config):
        assert(transform_config.is_execution_transform())
        tdg = self.get_tdg(transform_config, trace)
        gem5_out_dir = simulation_config.get_gem5_dir(
            tdg, self.get_sim_input_size())
        gem5_args = self.get_gem5_simulate_command(
            simulation_config=simulation_config,
            binary=self.get_replay_exe(transform_config, trace, 'exe'),
            outdir=gem5_out_dir,
            standalone=False,
        )
        # ! Always fast forward.
        gem5_args.append('--fast-forward=-1')
        # Do not add the tdg file, so that gem5 will simulate the binary.
        # For execution simulation, we would like to be in the exe_path.
        cwd = os.getcwd()
        os.chdir(self.get_sim_exe_path())
        if self.options.perf_command:
            gem5_args = ['perf', 'record'] + gem5_args
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
        # There is no sim_input_size for trace based simulation.
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
