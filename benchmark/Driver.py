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


def simulate(benchmark, tdg, gem5_config, debugs):
    benchmark.simulate(tdg, gem5_config, debugs)


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
        self.profile_jobs = dict()
        self.simpoint_jobs = dict()
        self.trace_jobs = dict()
        self.transform_jobs = dict()
        self.options = options
        self.gem5_config_manager = (
            Gem5ConfigureManager.Gem5ReplayConfigureManager(options))
        self.transform_manager = (
            TransformManager.TransformManager(options))

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

    def schedule_transform(self, job_scheduler, benchmark, transform_pass, debugs):
        name = benchmark.get_name()
        assert(transform_pass in Driver.PASS_NAME)
        pass_name = Driver.PASS_NAME[transform_pass]

        profile_file = benchmark.get_profile()
        traces = benchmark.get_traces()

        transform_configs = self.transform_manager.get_configs(transform_pass)
        for transform_config in transform_configs:
            transform_config_id = transform_config.get_id()
            tdgs = benchmark.get_tdgs(transform_config)

            assert(len(traces) == len(tdgs))
            if name not in self.transform_jobs:
                self.transform_jobs[name] = dict()
            assert(transform_config_id not in self.transform_jobs[name])
            self.transform_jobs[name][transform_config_id] = dict()

            for i in xrange(0, len(traces)):
                if self.options.trace_id:
                    if i not in self.options.trace_id:
                        # Ignore those traces if not specified
                        continue
                deps = list()
                if name in self.trace_jobs:
                    deps.append(self.trace_jobs[name])

                # Schedule the job.
                self.transform_jobs[name][transform_config_id][i] = (
                    job_scheduler.add_job(
                        '{name}.{transform_id}.transform'.format(
                            name=name,
                            transform_id=transform_config_id
                        ),
                        transform,
                        (
                            benchmark,
                            transform_config,
                            traces[i],
                            profile_file,
                            tdgs[i],
                            debugs,
                        ),
                        deps
                    )
                )

    def schedule_simulate(self, job_scheduler, benchmark, transform, debugs):
        for transform_config in self.transform_manager.get_configs(transform):
            self.schedule_simulate_for_transform_config(
                job_scheduler, benchmark, transform_config, debugs)

    def schedule_simulate_for_transform_config(self, job_scheduler, benchmark, transform_config, debugs):
        name = benchmark.get_name()
        tdgs = benchmark.get_tdgs(transform_config)
        transform_id = transform_config.get_id()
        transform = transform_config.get_transform()

        for i in xrange(0, len(tdgs)):
            if self.options.trace_id:
                if i not in self.options.trace_id:
                    # Ignore those traces if not specified
                    continue
            deps = list()
            if name in self.transform_jobs:
                if transform_id in self.transform_jobs[name]:
                    deps.append(self.transform_jobs[name][transform_id][i])

            gem5_configs = self.gem5_config_manager.get_configs(
                transform
            )
            for gem5_config in gem5_configs:
                job_scheduler.add_job(
                    '{name}.{transform_id}.simulate'.format(
                        name=name,
                        transform_id=transform_id,
                    ),
                    simulate,
                    (
                        benchmark,
                        tdgs[i],
                        gem5_config,
                        debugs,
                    ),
                    deps
                )


build_datagraph_debugs = {
    'inline-stream': [
        'ReplayPass',
        'InlineContextStreamPass',
        # 'DataGraph',
        'LoopUtils',
        # 'MemoryAccessPattern',
    ],
    'stream': [
        'ReplayPass',
        'StreamPass',
        # 'FunctionalStream',
        # 'DataGraph',
        'LoopUtils',
        # 'StreamPattern',
        # 'InductionVarStream',
        # 'TDGSerializer',
    ],
    'stream-prefetch': [
        'ReplayPass',
        'StreamPass',
        'StreamPrefetchPass',
        # 'DataGraph',
        'LoopUtils',
        # 'StreamPattern',
        # 'InductionVarStream',
        # 'TDGSerializer',
    ],
    'replay': [],
    'adfa': [],
    'simd': [],
}


simulate_datagraph_debugs = {
    'stream': [
        'McPATManager',
        # 'StreamEngine',
        # 'LLVMTraceCPU',
        # 'LLVMTraceCPUFetch',
        # 'LLVMTraceCPUCommit',
    ],
    'stream-prefetch': [
        # 'StreamEngine',
        # 'LLVMTraceCPU',
    ],
    'inline-stream': [],
    'replay': [
        # 'TDGLoadStoreQueue',
        # 'LLVMTraceCPU',
    ],
    'adfa': [
        # 'AbstractDataFlowAccelerator',
    ],
    'simd': [],
}


def choose_suite(options):
    if options.suite == 'spec':
        return SPEC2017.SPEC2017Benchmarks()
    elif options.suite == 'spu':
        return SPU.SPUBenchmarks(options.directory)
    elif options.suite == 'mach':
        return MachSuite.MachSuiteBenchmarks(options.directory)
    elif options.suite == 'hello':
        return TestHelloWorld.TestHelloWorldBenchmarks()
    elif options.suite == 'graph500':
        return Graph500.Graph500Benchmarks()
    elif options.suite == 'cortex':
        return CortexSuite.CortexSuite()
    elif options.suite == 'sdvbs':
        return SDVBS.SDVBSSuite()
    else:
        print('Unknown suite ' + options.suite)
        assert(False)


def main(options):
    job_scheduler = Util.JobScheduler(options.cores, 1)
    test_suite = choose_suite(options)
    benchmarks = test_suite.get_benchmarks()

    # Filter out other benchmarks not specified by the user.
    if options.benchmark is not None:
        benchmarks = [b for b in benchmarks if b.get_name()
                      in options.benchmark]

    driver = Driver(options)
    for benchmark in benchmarks:
        if options.build:
            benchmark.build_raw_bc()
        if options.profile:
            driver.schedule_profile(job_scheduler, benchmark)
        if options.simpoint:
            driver.schedule_simpoint(job_scheduler, benchmark)
        if options.trace:
            driver.schedule_trace(job_scheduler, benchmark)
        if options.build_datagraph:
            for transform_pass in options.transform_passes:
                driver.schedule_transform(
                    job_scheduler,
                    benchmark,
                    transform_pass,
                    build_datagraph_debugs[transform_pass])
        if options.simulate and (not options.hoffman2):
            for transform in options.transform_passes:
                driver.schedule_simulate(
                    job_scheduler,
                    benchmark,
                    transform,
                    simulate_datagraph_debugs[transform])
    job_scheduler.run()

    # If use hoffman2, prepare the cluster.
    if options.simulate and options.hoffman2:
        driver.simulate_hoffman2(benchmarks)

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
        driver.gem5_config_manager,
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
    parser.add_option('--directory', action='store',
                      type='string', dest='directory')
    parser.add_option('-p', '--pass', action='append',
                      type='string', dest='transform_passes')
    parser.add_option('--trace-id', type='string', action='callback',
                      dest='trace_id', callback=parse_trace_ids)
    parser.add_option('--benchmark', type='string', action='callback',
                      callback=parse_benchmarks, dest='benchmark')
    parser.add_option('-d', '--build-datagraph', action='store_true',
                      dest='build_datagraph', default=False)
    parser.add_option('-s', '--simulate', action='store_true',
                      dest='simulate', default=False)
    parser.add_option('--suite', action='store', type='string', dest='suite')
    # If true, the simuation is not performed, but prepare the hoffman2 cluster to do it.
    parser.add_option('--hoffman2', action='store_true',
                      dest='hoffman2', default=False)

    parser.add_option('--iw', action='store', type='int',
                      dest='iw', default=8)
    parser.add_option('--l1d-size', action='store', type='string',
                      dest='l1d_size', default='32kB')
    parser.add_option('--l1d-mshrs', action='store', type='int',
                      dest='l1d_mshrs', default=4)
    parser.add_option('--l1d-latency', action='store',
                      type='int', dest='l1d_latency', default=2)
    parser.add_option('--l1d-assoc', action='store', type='int',
                      dest='l1d_assoc', default=8)

    parser.add_option('--l1_5d', action='store_true',
                      dest='l1_5d', default=False)
    parser.add_option('--l1_5d-mshrs', action='store',
                      dest='l1_5d_mshrs', default=16)

    parser.add_option('--l2-size', action='store', type='string',
                      dest='l2_size', default='1MB')
    parser.add_option('--l2-mshrs', action='store', type='int',
                      dest='l2_mshrs', default=20)
    parser.add_option('--l2-latency', action='store',
                      type='int', dest='l2_latency', default=20)
    parser.add_option('--l2-assoc', action='store', type='int',
                      dest='l2_assoc', default=8)
    parser.add_option('--l2bus-width', action='store',
                      dest='l2bus_width', default=32)

    parser.add_option('--rp-prefetch', action='store_true',
                      dest='replay_prefetch', default=False)
    parser.add_option('--stream-choose-strategy', action='store', type='string',
                      dest='stream_choose_strategy', default='outer')
    parser.add_option('--se-prefetch', action='store_true',
                      dest='se_prefetch', default=False)
    parser.add_option('--se-oracle', action='store_true',
                      dest='se_oracle', default=False)
    parser.add_option('--se-ahead', action='callback', type='string',
                      callback=parse_stream_engine_maximum_run_ahead_length, dest='se_ahead')
    parser.add_option('--se-throttling', action='store',
                      type='string', dest='se_throttling', default='static')
    parser.add_option('--se-coalesce', action='store',
                      type='string', dest='se_coalesce', default='single')
    # Enable stream aware cache.
    parser.add_option('--se-l1d', action='store',
                      type='string', dest='se_l1d', default='original')

    # ADFA options.
    parser.add_option('--adfa-enable-speculation', action='store_true',
                      dest='adfa_enable_speculation', default=False)
    parser.add_option('--adfa-break-iv-dep', action='store_true',
                      dest='adfa_break_iv_dep', default=False)
    parser.add_option('--adfa-break-rv-dep', action='store_true',
                      dest='adfa_break_rv_dep', default=False)

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
    # Handle special values for the options.
    if options.transform_passes and 'all' in options.transform_passes:
        options.transform_passes = [
            'replay',
            'simd',
            'adfa',
            'stream',
        ]
    main(options)
