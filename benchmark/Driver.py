import Benchmark
import SPEC2017
import MachSuite
import TestHelloWorld
import SPU
import Graph500
import CortexSuite
import SDVBS

import Util
import StreamStatistics
import Constants as C

from Utils import BenchmarkResult
from Utils import Gem5ConfigureManager
from Utils import TransformManager

import os
import pickle
import glob

"""
Interface for test suite:
get_benchmarks() returns a list of benchmarks.

Interface for benchmark:
get_name() returns a unique name.
get_run_path() returns the path to run.
get_trace_ids() returns the trace ids.
build_raw_bc() build the raw bitcode.
trace() generates the trace.
transform(pass_name, trace, profile_file, tdg, debugs)
    transforms the trace by a specified pass.
simulate(tdg, result, debugs)
    simuates a transformed datagraph, and stores the simulation
    result to result
"""


# Top level function for scheduler.
def profile(benchmark):
    benchmark.profile()


def simpoint(benchmark):
    benchmark.simpoint()


def trace(benchmark):
    benchmark.trace()


def transform(benchmark, transform_config, trace, profile_file, tdg, debugs):
    benchmark.transform(transform_config, trace, profile_file, tdg, debugs)


def simulate(benchmark, tdg, simulation_config, debugs):
    benchmark.simulate(tdg, simulation_config, debugs)


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
            TransformManager.TransformManager(options))
        self.simulation_manager = (
            Gem5ConfigureManager.Gem5ReplayConfigureManager(options, self.transform_manager))

        self.benchmarks = self._choose_suite().get_benchmarks()
        # Filter out other benchmarks not specified by the user.
        if self.options.benchmark is not None:
            self.benchmarks = [b for b in self.benchmarks if b.get_name()
                               in self.options.benchmark]
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

    def _choose_suite(self):
        benchmark_args = Benchmark.BenchmarkArgs(
            self.transform_manager, self.simulation_manager
        )
        if self.options.suite == 'spec':
            return SPEC2017.SPEC2017Benchmarks(benchmark_args)
        elif self.options.suite == 'spu':
            return SPU.SPUBenchmarks(benchmark_args)
        elif self.options.suite == 'mach':
            return MachSuite.MachSuiteBenchmarks(benchmark_args)
        elif self.options.suite == 'hello':
            return TestHelloWorld.TestHelloWorldBenchmarks(benchmark_args)
        elif self.options.suite == 'graph500':
            return Graph500.Graph500Benchmarks(benchmark_args)
        elif self.options.suite == 'cortex':
            return CortexSuite.CortexSuite(benchmark_args)
        elif self.options.suite == 'sdvbs':
            return SDVBS.SDVBSSuite(benchmark_args)
        else:
            print('Unknown suite ' + self.options.suite)
            assert(False)

    def _schedule_and_run(self):
        job_scheduler = Util.JobScheduler(self.options.cores, 1)
        for benchmark in self.benchmarks:
            if self.options.build:
                benchmark.build_raw_bc()
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

            for trace in traces:
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
                            trace.get_trace_fn(),
                            profile_file,
                            tdgs[trace_id],
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
                        simulation_config,
                        simulation_config.get_debugs(),
                    ),
                    deps
                )


def main(options):

    driver = Driver(options)

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
        C.LLVM_TDG_DRIVER_DIR, 'Configurations', v) for v in vs]
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
        s = os.path.join(C.LLVM_TDG_DRIVER_DIR, 'Configurations/Simulation', v)
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
    default_cores = os.getenv('LLVM_TDG_CPUS')
    if default_cores is not None:
        default_cores = int(default_cores)
    else:
        default_cores = 8
    parser = optparse.OptionParser()
    parser.add_option('-j', '--cores', action='store',
                      type='int', dest='cores', default=default_cores)
    parser.add_option('-b', '--build', action='store_true',
                      dest='build', default=False)
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

    parser.add_option('--trans-configs', type='string', action='callback', default='',
                      dest='transforms', callback=parse_transform_configurations)
    parser.add_option('--sim-configs', type='string', action='callback', default='',
                      dest='simulations', callback=parse_simulate_configurations)

    parser.add_option('--trace-id', type='string', action='callback',
                      dest='trace_id', callback=parse_trace_ids)
    parser.add_option('--benchmark', type='string', action='callback',
                      callback=parse_benchmarks, dest='benchmark')
    parser.add_option('--suite', action='store', type='string', dest='suite')
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
