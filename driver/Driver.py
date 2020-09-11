from BenchmarkDrivers import Benchmark
from BenchmarkDrivers import MultiProgramBenchmark
from BenchmarkDrivers import SPEC2017
from BenchmarkDrivers import Parsec
from BenchmarkDrivers import Rodinia
from BenchmarkDrivers import MachSuite
from BenchmarkDrivers import TestHelloWorld
from BenchmarkDrivers import SPU
from BenchmarkDrivers import Graph500
from BenchmarkDrivers import Parboil
from BenchmarkDrivers import CortexSuite
from BenchmarkDrivers import CortexSuiteValid
from BenchmarkDrivers import SDVBS
from BenchmarkDrivers import SDVBSValid
from BenchmarkDrivers import VerticalSuite
from BenchmarkDrivers import MediaBench
from BenchmarkDrivers import GemForgeMicroSuite
from BenchmarkDrivers import GenerateTestInputs

import Util
import JobScheduler
import Constants as C

from Utils import BenchmarkResult
from Utils import Gem5ConfigureManager
from Utils import TransformManager

from ProcessingScripts import ADFAExperiments
from ProcessingScripts import StreamExperiments
from ProcessingScripts import ValidExperiments
from ProcessingScripts import LibraryInstExperiments
from ProcessingScripts import SpeedupExperiments

import os
import random
import pickle
import glob

"""
Interface for test suite:
get_benchmarks() returns a list of benchmarks.

Interface for benchmark:
get_name() returns a unique name.
get_run_path() returns the path to run.
build_raw_bc() build the raw bitcode.
trace() generates the trace.
transform(pass_name, trace, profile_file, tdg)
    transforms the trace by a specified pass.
simulate(trace, transform_config, simulation_config)
    simuates a transformed datagraph.
"""


# Top level function for scheduler.
def perf(benchmark):
    benchmark.perf()


def opt_analyze(benchmark):
    benchmark.opt_analyze()


def profile(benchmark):
    benchmark.profile()


def simpoint(benchmark):
    benchmark.simpoint()


def trace(benchmark):
    benchmark.trace()


def transform(benchmark, transform_config, trace, tdg):
    benchmark.transform(transform_config, trace, tdg)


def simulate(benchmark, trace, transform_config, simulation_config):
    benchmark.simulate(trace, transform_config, simulation_config)


def mcpat(benchmark, tdg, transform_config, simulation_config):
    benchmark.mcpat(tdg, transform_config, simulation_config)


class Driver:

    PASS_NAME = {
        'replay': 'replay',
        'adfa': 'abs-data-flow-acc-pass',
        'stream': 'stream-pass',
        'stream-prefetch': 'stream-prefetch-pass',
        'inline-stream': 'inline-stream-pass',
        'simd': 'simd-pass',
    }

    def __init__(self, options):
        self.options = options

        self.transform_manager = (
            TransformManager.TransformManager(options.transforms))
        self.simulation_manager = (
            Gem5ConfigureManager.Gem5ReplayConfigureManager(options.simulations, self.transform_manager))

        self.benchmarks = self._init_benchmarks()
        # Filter out other benchmarks not specified by the user.
        if self.options.benchmark is not None:
            self.benchmarks = [b for b in self.benchmarks if b.get_name()
                               in self.options.benchmark]
        if self.options.exclude_benchmark is not None:
            self.benchmarks = [b for b in self.benchmarks if b.get_name(
            ) not in self.options.exclude_benchmark]
        self.perf_jobs = dict()
        self.opt_analyze_jobs = dict()
        self.profile_jobs = dict()
        self.simpoint_jobs = dict()
        self.trace_jobs = dict()
        self.transform_jobs = dict()
        self.simulate_jobs = dict()

        # Remember to initialize the transform_path
        for benchmark in self.benchmarks:
            for transform_config in self.transform_manager.get_all_configs():
                transform_id = transform_config.get_transform_id()
                benchmark.init_transform_path(transform_id)

        self._schedule_and_run()

    def get_unique_id(self):
        # Get a uid for this run.
        # Use the suites and transforms
        return '{ss}.{ts}'.format(
            ss='.'.join(self._get_suites()),
            ts='.'.join(self.transform_manager.get_all_transform_ids())
        )

    def _get_suites(self):
        suites = list()
        for b in self.options.benchmark:
            # Benchmark has name suite.benchmark.
            idx = b.find('.')
            if idx == -1:
                print('Illegal suite for benchmark {b}'.format(b=b))
            suite = b[:idx]
            if suite not in suites:
                suites.append(suite)
        suites.sort()
        return suites

    def _init_benchmarks(self):
        benchmark_args = Benchmark.BenchmarkArgs(
            self.transform_manager, self.simulation_manager, self.options
        )
        benchmarks = list()
        for suite_name in self._get_suites():
            suite = None
            if suite_name == 'spec':
                suite = SPEC2017.SPEC2017Benchmarks(benchmark_args)
            elif suite_name == 'parsec':
                suite = Parsec.ParsecSuite(benchmark_args)
            elif suite_name == 'rodinia':
                suite = Rodinia.RodiniaSuite(benchmark_args)
            elif suite_name == 'spu':
                suite = SPU.SPUBenchmarks(benchmark_args)
            elif suite_name == 'mach':
                suite = MachSuite.MachSuiteBenchmarks(benchmark_args)
            elif suite_name == 'hello':
                suite = TestHelloWorld.TestHelloWorldBenchmarks(benchmark_args)
            elif suite_name == 'graph':
                suite = Graph500.Graph500Benchmarks(benchmark_args)
            elif suite_name == 'parboil':
                suite = Parboil.ParboilBenchmarks(benchmark_args)
            elif suite_name == 'cortex':
                suite = CortexSuite.CortexSuite(benchmark_args)
            elif suite_name == 'cortex.valid':
                suite = CortexSuiteValid.CortexValidSuite(benchmark_args)
            elif suite_name == 'sdvbs':
                suite = SDVBS.SDVBSSuite(benchmark_args)
            elif suite_name == 'sdvbs.valid':
                suite = SDVBSValid.SDVBSValidSuite(benchmark_args)
            elif suite_name == 'vert':
                suite = VerticalSuite.VerticalSuite(benchmark_args)
            elif suite_name == 'mb':
                suite = MediaBench.MediaBenchSuite(benchmark_args)
            elif suite_name == 'gfm':
                suite = GemForgeMicroSuite.GemForgeMicroSuite(benchmark_args)
            elif suite_name == 'test':
                suite = GenerateTestInputs.TestInputSuite(benchmark_args)
            else:
                print('Unknown suite {s}'.format(s=suite_name))
                assert(False)
            benchmarks += suite.get_benchmarks()
        # Sort the benchmarks by name.
        benchmarks.sort(key=lambda x: x.get_name())

        """
        If multi-program is greater than 1, then we group benchmarks into
        MultiProgramBenchmarks. Extra benchmarks will be ignored!
        """
        if self.options.multi_programs > 1:
            # Get a fixed suffle.
            benchmark_idx_shuffled = range(len(benchmarks))
            random.Random(1).shuffle(benchmark_idx_shuffled)
            multi_program_benchmarks = list()
            current_idx = 0
            while current_idx + self.options.multi_programs <= len(benchmarks):
                benchmark_list = [
                    benchmarks[benchmark_idx_shuffled[current_idx + i]]
                    for i in range(self.options.multi_programs)
                ]
                multi_program_benchmarks.append(
                    MultiProgramBenchmark.MultiProgramBenchmark(
                        benchmark_args, benchmark_list)
                )
                current_idx += self.options.multi_programs
            # Replace our benchmark list.
            benchmarks = multi_program_benchmarks

        return benchmarks

    def _schedule_and_run(self):
        job_scheduler = JobScheduler.JobScheduler(
            self.get_unique_id(), self.options.cores, 1)
        for benchmark in self.benchmarks:
            if self.options.build:
                benchmark.build_raw_bc()
            if self.options.opt_analyze:
                self.schedule_opt_analyze(job_scheduler, benchmark)
            if self.options.perf:
                self.schedule_perf(job_scheduler, benchmark)
            if self.options.profile:
                self.schedule_profile(job_scheduler, benchmark)
            if self.options.simpoint:
                self.schedule_simpoint(job_scheduler, benchmark)
            if self.options.trace:
                self.schedule_trace(job_scheduler, benchmark)
            if self.options.build_datagraph:
                self.schedule_transform(job_scheduler, benchmark)
            if self.options.simulate and (not self.options.hoffman2):
                self.schedule_simulate(job_scheduler, benchmark)
            if self.options.mcpat:
                self.schedule_mcpat(job_scheduler, benchmark)
        job_scheduler.run()

    def schedule_profile(self, job_scheduler, benchmark):
        name = benchmark.get_name()
        self.profile_jobs[name] = job_scheduler.add_job(name + '.profile', profile,
                                                        (benchmark, ), list())

    def schedule_perf(self, job_scheduler, benchmark):
        name = benchmark.get_name()
        self.perf_jobs[name] = job_scheduler.add_job(name + '.perf', perf,
                                                     (benchmark, ), list())

    def schedule_opt_analyze(self, job_scheduler, benchmark):
        name = benchmark.get_name()
        self.opt_analyze_jobs[name] = job_scheduler.add_job(name + '.opt_analyze', opt_analyze,
                                                            (benchmark, ), list())

    def schedule_simpoint(self, job_scheduler, benchmark):
        name = benchmark.get_name()
        # Simpoint job is dependent on profile job.
        deps = list()
        if name in self.profile_jobs:
            deps.append(self.profile_jobs[name])
        self.simpoint_jobs[name] = job_scheduler.add_job(
            name=name + '.simpoint',
            job=simpoint,
            args=(benchmark, ),
            deps=deps)

    def schedule_trace(self, job_scheduler, benchmark):
        name = benchmark.get_name()
        assert(name not in self.trace_jobs)
        deps = list()
        if name in self.simpoint_jobs:
            deps.append(self.simpoint_jobs[name])
        self.trace_jobs[name] = (
            job_scheduler.add_job(name + '.trace', trace,
                                  (benchmark, ), deps)
        )

    def schedule_transform(self, job_scheduler, benchmark):
        name = benchmark.get_name()
        traces = benchmark.get_traces()

        for transform_config in self.transform_manager.get_all_configs():
            transform_id = transform_config.get_transform_id()
            tdgs = benchmark.get_tdgs(transform_config)

            assert(len(traces) == len(tdgs))
            if name not in self.transform_jobs:
                self.transform_jobs[name] = dict()
            assert(transform_id not in self.transform_jobs[name])
            self.transform_jobs[name][transform_id] = dict()

            for i in xrange(len(traces)):
                trace = traces[i]
                trace_id = trace.get_trace_id()
                deps = list()
                if name in self.trace_jobs:
                    deps.append(self.trace_jobs[name])

                # Schedule the job.
                self.transform_jobs[name][transform_id][trace_id] = (
                    job_scheduler.add_job(
                        name='{name}.{transform_id}.transform'.format(
                            name=name,
                            transform_id=transform_id
                        ),
                        job=transform,
                        args=(
                            benchmark,
                            transform_config,
                            trace,
                            tdgs[i],
                        ),
                        deps=deps
                    )
                )

    def schedule_simulate(self, job_scheduler, benchmark):
        self.simulate_jobs[benchmark.get_name()] = dict()
        for transform_config in self.transform_manager.get_all_configs():
            self.schedule_simulate_for_transform_config(
                job_scheduler, benchmark, transform_config)

    def schedule_simulate_for_transform_config(self, job_scheduler, benchmark, transform_config):
        name = benchmark.get_name()
        traces = benchmark.get_traces()
        transform_id = transform_config.get_transform_id()
        self.simulate_jobs[name][transform_id] = dict()

        for trace in traces:
            trace_id = trace.get_trace_id()
            self.simulate_jobs[name][transform_id][trace_id] = dict()
            deps = list()
            if name in self.transform_jobs:
                if transform_id in self.transform_jobs[name]:
                    deps.append(
                        self.transform_jobs[name][transform_id][trace_id])

            simulation_configs = self.simulation_manager.get_configs(
                transform_id
            )
            for simulation_config in simulation_configs:
                simulation_id = simulation_config.get_simulation_id()
                self.simulate_jobs[name][transform_id][trace_id][simulation_id] = job_scheduler.add_job(
                    name='{name}.{transform_id}.{simulation_id}'.format(
                        name=name,
                        transform_id=transform_id,
                        simulation_id=simulation_config.get_simulation_id()
                    ),
                    job=simulate,
                    args=(
                        benchmark,
                        trace,
                        transform_config,
                        simulation_config,
                    ),
                    deps=deps,
                    # Use a 20 hour timeout for simulation to avoid deadlock.
                    timeout=20*60*60
                )

    def schedule_mcpat(self, job_scheduler, benchmark):
        name = benchmark.get_name()
        traces = benchmark.get_traces()
        for transform_config in self.transform_manager.get_all_configs():
            tdgs = benchmark.get_tdgs(transform_config)
            assert(len(tdgs) == len(traces))
            transform_id = transform_config.get_transform_id()
            simulation_configs = self.simulation_manager.get_configs(
                transform_id)
            for i in range(0, len(tdgs)):
                trace_id = traces[i].get_trace_id()
                tdg = tdgs[i]
                for simulation_config in simulation_configs:
                    simulation_id = simulation_config.get_simulation_id()
                    # Add dependence on the simulation job.
                    deps = list()
                    if name in self.simulate_jobs:
                        deps.append(
                            self.simulate_jobs[name][transform_id][trace_id][simulation_id])
                    job_scheduler.add_job(
                        name='{name}.{transform_id}.{simulation_id}.{trace_id}.mcpat'.format(
                            name=name,
                            transform_id=transform_id,
                            simulation_id=simulation_id,
                            trace_id=trace_id,
                        ),
                        job=mcpat,
                        args=(
                            benchmark,
                            tdg,
                            transform_config,
                            simulation_config
                        ),
                        deps=deps,
                    )

    def load_simulation_results(self):
        self.simulation_results = list()
        for gem_forge_system_config in self.simulation_manager.get_all_gem_forge_system_configs():
            transform_config = gem_forge_system_config.transform_config
            simulation_config = gem_forge_system_config.simulation_config
            for benchmark in self.benchmarks:
                for trace in benchmark.get_traces():
                    self.simulation_results.append(BenchmarkResult.SimulationResult(
                        benchmark, trace, transform_config, simulation_config))

    """
    Lookup a specific simulation result.
    """

    def get_simulation_result(self, benchmark, trace, transform_config, simulation_config):
        for simulation_result in self.simulation_results:
            if simulation_result.benchmark != benchmark:
                continue
            if simulation_result.trace != trace:
                continue
            if simulation_result.transform_config != transform_config:
                continue
            if simulation_result.simulation_config != simulation_config:
                continue
            return simulation_result
        assert(False)

    def clean(self):
        for benchmark in self.benchmarks:
            benchmark.clean(options.clean)


def main(options):

    driver = Driver(options)
    if options.analyze != '':
        driver.load_simulation_results()
        if options.analyze == 'fractal':
            ADFAExperiments.analyze(driver)
        if options.analyze == 'stream':
            StreamExperiments.analyze(driver)
        if options.analyze == 'valid':
            ValidExperiments.analyze(driver)
        if options.analyze == 'library':
            LibraryInstExperiments.analyze(driver)
        if options.analyze == 'speedup':
            SpeedupExperiments.analyze(driver)
    if options.clean != '':
        driver.clean()


def parse_benchmarks(option, opt, value, parser):
    setattr(parser.values, option.dest, value.split(','))


def parse_trace_ids(option, opt, value, parser):
    vs = value.split(',')
    setattr(parser.values, option.dest, [int(x) for x in vs])


def parse_stream_plot(option, opt, value, parser):
    setattr(parser.values, option.dest, value.split(','))


def parse_transform_configurations(option, opt, value, parser):
    vs = value.split(',')
    full_paths = [os.path.join(
        C.LLVM_TDG_DRIVER_DIR, 'Configurations', v + '.json') for v in vs]
    for full_path in full_paths:
        if not os.path.isfile(full_path):
            print('Transform configuration does not exist: {path}.'.format(
                path=full_path))
            assert(False)
    setattr(parser.values, option.dest, full_paths)


def parse_simulate_configurations(option, opt, value, parser):
    vs = value.split(',')
    full_paths = list()
    for v in vs:
        s = os.path.join(C.LLVM_TDG_DRIVER_DIR,
                         'Configurations/Simulation', v + '.json')
        print s
        ss = glob.glob(s)
        for full_path in ss:
            full_paths.append(full_path)
    print('Simulation Configurtions {fp}'.format(fp=full_paths))
    for full_path in full_paths:
        if not os.path.isfile(full_path):
            print('Simulation configuration does not exist: {path}.'.format(
                path=full_path))
            assert(False)
    setattr(parser.values, option.dest, full_paths)


if __name__ == '__main__':
    import optparse
    parser = optparse.OptionParser()
    parser.add_option('-j', '--cores', action='store',
                      type='int', dest='cores', default=8)
    parser.add_option('--build', action='store_true',
                      dest='build', default=False)
    parser.add_option('--perf', action='store_true',
                      dest='perf', default=False)
    parser.add_option('--profile', action='store_true',
                      dest='profile', default=False)
    parser.add_option('--simpoint', action='store_true',
                      dest='simpoint', default=False)
    parser.add_option('--simpoint-mode', action='store', type='choice',
                      dest='simpoint_mode', default='none',
                      choices=['none', 'fix', 'region'],
                      help='simpoint mode: fix, region [default: %default]')
    parser.add_option('--trace', action='store_true',
                      dest='trace', default=False)
    parser.add_option('-d', '--build-datagraph', action='store_true',
                      dest='build_datagraph', default=False)
    parser.add_option('-s', '--simulate', action='store_true',
                      dest='simulate', default=False)
    parser.add_option('--mcpat', action='store_true',
                      dest='mcpat', default=False)
    parser.add_option('--analyze', type='string', action='store',
                      dest='analyze', default='')
    parser.add_option('--clean', type='string', action='store',
                      dest='clean', default='')
    parser.add_option('--opt-analyze', type='string',
                      action='store', dest='opt_analyze', default='')

    parser.add_option('-t', '--trans-configs', type='string', action='callback', default='',
                      dest='transforms', callback=parse_transform_configurations)
    parser.add_option('--transform-debug', type='string',
                      action='store', dest='transform_debug', default=None)
    parser.add_option('--transform-text', action='store_true',
                      dest='transform_text', default=False)
    parser.add_option('--sim-configs', type='string', action='callback', default='',
                      dest='simulations', callback=parse_simulate_configurations)
    parser.add_option('--perf-command', action='store_true',
                      dest='perf_command', default=False)
    parser.add_option('--region-simpoint', action='store_true',
                      dest='region_simpoint', default=False)
    parser.add_option('--fake-trace', action='store_true',
                      dest='fake_trace', default=False)

    parser.add_option('-b', '--benchmark', type='string', action='callback',
                      dest='benchmark', callback=parse_benchmarks)
    parser.add_option('--exclude-benchmark', type='string', action='callback',
                      dest='exclude_benchmark', callback=parse_benchmarks)
    parser.add_option('--trace-id', type='string', action='callback',
                      dest='trace_id', callback=parse_trace_ids)

    parser.add_option('--input-size', type='string', action='store',
                      dest='input_name', default=None)
    parser.add_option('--input-threads', type='int', action='store',
                      dest='input_threads', default=1)
    parser.add_option('--sim-input-size', type='string', action='store',
                      dest='sim_input_name', default=None)

    parser.add_option('--stream-plot', type='string', action='callback',
                      dest='stream_plot', callback=parse_stream_plot)

    parser.add_option('--multi-program', type='int',
                      action='store', dest='multi_programs', default=1)

    # Parse gem5 debug flag.
    parser.add_option('--gem5-debug', type='string',
                      action='store', dest='gem5_debug', default=None)
    parser.add_option('--gem5-debug-file', type='string',
                      action='store', dest='gem5_debug_file', default=None)
    parser.add_option('--gem5-debug-start', type='int',
                      action='store', dest='gem5_debug_start', default=None)
    parser.add_option('--gem5-max-ticks', type='int',
                      action='store', dest='gem5_max_ticks', default=None)
    parser.add_option('--gem5-max-insts', type='int',
                      action='store', dest='gem5_max_insts', default=None)

    # If true, the simuation is not performed, but prepare the hoffman2 cluster to do it.
    parser.add_option('--hoffman2', action='store_true',
                      dest='hoffman2', default=False)

    (options, args) = parser.parse_args()
    main(options)
