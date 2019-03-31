import Benchmark
import MultiProgramBenchmark
import SPEC2017
import MachSuite
import TestHelloWorld
import SPU
import Graph500
import CortexSuite
import SDVBS
import VerticalSuite
from BenchmarkDrivers import GemForgeMicroSuite
import GenerateTestInputs

import Util
import StreamStatistics
import Constants as C

from Utils import BenchmarkResult
from Utils import Gem5ConfigureManager
from Utils import TransformManager

from ProcessingScripts import ADFAExperiments
from ProcessingScripts import StreamExperiments
from ProcessingScripts import ValidExperiments
from ProcessingScripts import LibraryInstExperiments

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
transform(pass_name, trace, profile_file, tdg, debugs)
    transforms the trace by a specified pass.
simulate(tdg, result, debugs)
    simuates a transformed datagraph, and stores the simulation
    result to result
"""


# Top level function for scheduler.
def perf(benchmark):
    benchmark.perf()


def profile(benchmark):
    benchmark.profile()


def simpoint(benchmark):
    benchmark.simpoint()


def trace(benchmark):
    benchmark.trace()


def transform(benchmark, transform_config, trace, profile_file, tdg, debugs):
    benchmark.transform(transform_config, trace, profile_file, tdg, debugs)


def simulate(benchmark, tdg, transform_config, simulation_config):
    benchmark.simulate(tdg, transform_config, simulation_config)


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
        self.perf_jobs = dict()
        self.profile_jobs = dict()
        self.simpoint_jobs = dict()
        self.trace_jobs = dict()
        self.transform_jobs = dict()

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
            ss='.'.join(self.options.suite),
            ts='.'.join(self.transform_manager.get_transforms())
        )

    def _init_benchmarks(self):
        benchmark_args = Benchmark.BenchmarkArgs(
            self.transform_manager, self.simulation_manager
        )
        benchmarks = list()
        for suite_name in self.options.suite:
            suite = None
            if suite_name == 'spec':
                suite = SPEC2017.SPEC2017Benchmarks(benchmark_args)
            elif suite_name == 'spu':
                suite = SPU.SPUBenchmarks(benchmark_args)
            elif suite_name == 'mach':
                suite = MachSuite.MachSuiteBenchmarks(benchmark_args)
            elif suite_name == 'hello':
                suite = TestHelloWorld.TestHelloWorldBenchmarks(benchmark_args)
            elif suite_name == 'graph500':
                suite = Graph500.Graph500Benchmarks(benchmark_args)
            elif suite_name == 'cortex':
                suite = CortexSuite.CortexSuite(benchmark_args)
            elif suite_name == 'sdvbs':
                suite = SDVBS.SDVBSSuite(benchmark_args)
            elif suite_name == 'vert':
                suite = VerticalSuite.VerticalSuite(benchmark_args)
            elif suite_name == 'gfm':
                suite = GemForgeMicroSuite.GemForgeMicroSuite(benchmark_args)
            elif suite_name == 'test':
                suite = GenerateTestInputs.TestInputSuite(benchmark_args)
            else:
                print('Unknown suite ' + ','.join(self.options.suite))
                assert(False)
            benchmarks += suite.get_benchmarks()

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
        job_scheduler = Util.JobScheduler(
            self.get_unique_id(), self.options.cores, 1)
        for benchmark in self.benchmarks:
            if self.options.build:
                benchmark.build_raw_bc()
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
        job_scheduler.run()

    def schedule_profile(self, job_scheduler, benchmark):
        name = benchmark.get_name()
        self.profile_jobs[name] = job_scheduler.add_job(name + '.profile', profile,
                                                        (benchmark, ), list())

    def schedule_perf(self, job_scheduler, benchmark):
        name = benchmark.get_name()
        self.perf_jobs[name] = job_scheduler.add_job(name + '.perf', perf,
                                                     (benchmark, ), list())

    def schedule_simpoint(self, job_scheduler, benchmark):
        name = benchmark.get_name()
        # Simpoint job is dependent on profile job.
        deps = list()
        if name in self.profile_jobs:
            deps.append(self.profile_jobs[name])
        self.simpoint_jobs[name] = job_scheduler.add_job(name + '.simpoint', simpoint,
                                                         (benchmark, ), deps)

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

        profile_file = benchmark.get_profile()
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
                if self.options.trace_id:
                    if trace_id not in self.options.trace_id:
                        # Ignore those traces if not specified
                        continue
                deps = list()
                if name in self.trace_jobs:
                    deps.append(self.trace_jobs[name])

                # Schedule the job.
                self.transform_jobs[name][transform_id][trace_id] = (
                    job_scheduler.add_job(
                        '{name}.{transform_id}.transform'.format(
                            name=name,
                            transform_id=transform_id
                        ),
                        transform,
                        (
                            benchmark,
                            transform_config,
                            trace,
                            profile_file,
                            tdgs[i],
                            transform_config.get_debugs(),
                        ),
                        deps
                    )
                )

    def schedule_simulate(self, job_scheduler, benchmark):
        for transform_config in self.transform_manager.get_all_configs():
            self.schedule_simulate_for_transform_config(
                job_scheduler, benchmark, transform_config)

    def schedule_simulate_for_transform_config(self, job_scheduler, benchmark, transform_config):
        name = benchmark.get_name()
        tdgs = benchmark.get_tdgs(transform_config)
        transform_id = transform_config.get_transform_id()

        for i in xrange(0, len(tdgs)):
            if self.options.trace_id:
                if i not in self.options.trace_id:
                    # Ignore those traces if not specified
                    continue
            deps = list()
            if name in self.transform_jobs:
                if transform_id in self.transform_jobs[name]:
                    deps.append(self.transform_jobs[name][transform_id][i])

            simulation_configs = self.simulation_manager.get_configs(
                transform_id
            )
            for simulation_config in simulation_configs:
                job_scheduler.add_job(
                    '{name}.{transform_id}.{simulation_id}'.format(
                        name=name,
                        transform_id=transform_id,
                        simulation_id=simulation_config.get_simulation_id()
                    ),
                    simulate,
                    (
                        benchmark,
                        tdgs[i],
                        transform_config,
                        simulation_config,
                    ),
                    deps
                )

    def load_simulation_results(self):
        self.simulation_results = list()
        for benchmark in self.benchmarks:
            for transform_config in self.transform_manager.get_all_configs():
                transform_id = transform_config.get_transform_id()
                for simulation_config in self.simulation_manager.get_configs(transform_id):
                    for trace in benchmark.get_traces():
                        self.simulation_results.append(BenchmarkResult.SimulationResult(
                            benchmark, trace, transform_config, simulation_config))

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
    if options.clean != '':
        driver.clean()

    benchmark_stream_statistics = dict()

    for benchmark in benchmarks:
        if 'stream' in options.transform_passes:
            stream_tdgs = benchmark.get_tdgs(
                driver.transform_manager.get_configs('stream')[0])

            stream_stats = StreamStatistics.StreamStatistics(
                benchmark.get_name(),
                stream_tdgs
            )
            benchmark_stream_statistics[benchmark.get_name()] = stream_stats
            print('-------------------------- ' + benchmark.get_name())

    if benchmark_stream_statistics:
        if options.dump_stream_stats:
            stream_stats_pickle = 'Plots/data/{suite}.stream_stats.dat'.format(
                suite=options.suite)
            with open(stream_stats_pickle, mode='wb') as f:
                pickle.dump(benchmark_stream_statistics, f)
        # StreamStatistics.StreamStatistics.print_benchmark_stream_breakdown_coarse(
        #     benchmark_stream_statistics)
     #    StreamStatistics.StreamStatistics.print_benchmark_stream_breakdown_indirect(
     #        benchmark_stream_statistics)
     #    StreamStatistics.StreamStatistics.print_benchmark_stream_paths(
     #        benchmark_stream_statistics)
        if options.dump_stream_breakdown:
            StreamStatistics.StreamStatistics.print_benchmark_stream_breakdown(
                benchmark_stream_statistics)
        StreamStatistics.StreamStatistics.print_benchmark_chosen_stream_percentage(
            benchmark_stream_statistics)
        StreamStatistics.StreamStatistics.print_benchmark_chosen_stream_length(
            benchmark_stream_statistics)
        StreamStatistics.StreamStatistics.print_benchmark_chosen_stream_indirect(
            benchmark_stream_statistics)
     #    StreamStatistics.StreamStatistics.print_benchmark_chosen_stream_loop_path(
     #        benchmark_stream_statistics)
     #    StreamStatistics.StreamStatistics.print_benchmark_chosen_stream_configure_level(
     #        benchmark_stream_statistics)
        StreamStatistics.StreamStatistics.print_benchmark_static_max_n_alive_streams(
            benchmark_stream_statistics)

    suite_result = BenchmarkResult.SuiteResult(
        options.suite,
        benchmarks,
        driver.transform_manager,
        driver.simulation_manager,
        options.transform_passes)

    suite_result.show_region_stats()

    if options.dump_suite_results:
        folder = 'Plots/data'
        suite_result.pickle(folder)
    energy_attribute = BenchmarkResult.BenchmarkResult.get_attribute_energy()
    se_energy_attribute = BenchmarkResult.BenchmarkResult.get_attribute_se_energy()
    time_attribute = BenchmarkResult.BenchmarkResult.get_attribute_time()
    suite_result.compare(
        [energy_attribute, se_energy_attribute, time_attribute])

    if len(options.transform_passes) > 1 and 'replay' in options.transform_passes:
        for transform in options.transform_passes:
            if transform == 'replay':
                continue
            for transform_config in driver.transform_manager.get_configs(transform):
                suite_result.compare_transform_speedup(transform_config)
                suite_result.compare_transform_energy(transform_config)

    if options.dump_stream_placement:
        if 'stream' in options.transform_passes:
            for transform_config in driver.transform_manager.get_configs('stream'):
                suite_result.show_stream_placement(transform_config)
                suite_result.show_hit_lower(transform_config)
                suite_result.show_hit_higher(transform_config)

    if options.dump_cache_hits:
        if 'stream' in options.transform_passes:
            for transform_config in driver.transform_manager.get_configs('stream'):
                suite_result.show_cache_hits(transform_config)
                # suite_result.show_cache_coalesce_hits(transform_config)


def parse_suites(option, opt, value, parser):
    setattr(parser.values, option.dest, value.split(','))


def parse_benchmarks(option, opt, value, parser):
    setattr(parser.values, option.dest, value.split(','))


def parse_trace_ids(option, opt, value, parser):
    vs = value.split(',')
    setattr(parser.values, option.dest, [int(x) for x in vs])


def parse_stream_engine_maximum_run_ahead_length(option, opt, value, parser):
    vs = value.split(',')
    setattr(parser.values, option.dest, [int(x) for x in vs])


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
    # full_paths = [os.path.join(
    #     C.LLVM_TDG_DRIVER_DIR, 'Configurations/Simulation', v) for v in vs]
    full_paths = list()
    for v in vs:
        s = os.path.join(C.LLVM_TDG_DRIVER_DIR,
                         'Configurations/Simulation', v + '.json')
        ss = glob.glob(s)
        for full_path in ss:
            full_paths.append(full_path)
    print(full_paths)
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
    parser.add_option('-b', '--build', action='store_true',
                      dest='build', default=False)
    parser.add_option('--perf', action='store_true',
                      dest='perf', default=False)
    parser.add_option('--profile', action='store_true',
                      dest='profile', default=False)
    parser.add_option('--simpoint', action='store_true',
                      dest='simpoint', default=False)
    parser.add_option('-t', '--trace', action='store_true',
                      dest='trace', default=False)
    parser.add_option('-d', '--build-datagraph', action='store_true',
                      dest='build_datagraph', default=False)
    parser.add_option('-s', '--simulate', action='store_true',
                      dest='simulate', default=False)
    parser.add_option('--analyze', type='string', action='store',
                      dest='analyze', default='')
    parser.add_option('--clean', type='string', action='store',
                      dest='clean', default='')

    parser.add_option('--trans-configs', type='string', action='callback', default='',
                      dest='transforms', callback=parse_transform_configurations)
    parser.add_option('--sim-configs', type='string', action='callback', default='',
                      dest='simulations', callback=parse_simulate_configurations)

    parser.add_option('--suite', type='string', action='callback',
                      dest='suite', callback=parse_suites)
    parser.add_option('--benchmark', type='string', action='callback',
                      dest='benchmark', callback=parse_benchmarks)
    parser.add_option('--trace-id', type='string', action='callback',
                      dest='trace_id', callback=parse_trace_ids)

    parser.add_option('--multi-program', type='int',
                      action='store', dest='multi_programs', default=1)

    # If true, the simuation is not performed, but prepare the hoffman2 cluster to do it.
    parser.add_option('--hoffman2', action='store_true',
                      dest='hoffman2', default=False)

    # Dump infos.
    parser.add_option('--dump-stream-placement', action='store_true',
                      dest='dump_stream_placement', default=False)
    parser.add_option('--dump-cache-hits', action='store_true',
                      dest='dump_cache_hits', default=False)

    parser.add_option('--dump-stream-breakdown', action='store_true',
                      dest='dump_stream_breakdown', default=False)

    parser.add_option('--dump-stream-stats', action='store_true',
                      dest='dump_stream_stats', default=False)

    parser.add_option('--dump-suite-results', action='store_true',
                      dest='dump_suite_results', default=False)

    (options, args) = parser.parse_args()
    main(options)
