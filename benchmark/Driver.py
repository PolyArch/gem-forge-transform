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

import os

"""
Interface for test suite:
get_benchmarks() returns a list of benchmarks.

Interface for benchmark:
get_name() returns a unique name.
get_run_path() returns the path to run.
get_n_traces() returns the number of traces.
build_raw_bc() build the raw bitcode.
trace() generates the trace.
transform(pass_name, trace, profile_file, tdg, debugs)
    transforms the trace by a specified pass.
simulate(tdg, result, debugs)
    simuates a transformed datagraph, and stores the simulation
    result to result
"""


# Top level function for scheduler.
def trace(benchmark):
    benchmark.trace()


def transform(benchmark, pass_name, trace, profile_file, tdg, debugs):
    benchmark.transform(pass_name, trace, profile_file, tdg, debugs)


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
        self.trace_jobs = dict()
        self.transform_jobs = dict()
        self.options = options
        self.gem5_config_manager = (
            Gem5ConfigureManager.Gem5ReplayConfigureManager(options))

    def schedule_trace(self, job_scheduler, benchmark):
        name = benchmark.get_name()
        assert(name not in self.trace_jobs)
        # No dependence for the trace job.
        self.trace_jobs[name] = (
            job_scheduler.add_job(name + '.trace', trace,
                                  (benchmark, ), list())
        )

    def schedule_transform(self, job_scheduler, benchmark, transform_pass, debugs):
        name = benchmark.get_name()
        assert(transform_pass in Driver.PASS_NAME)
        pass_name = Driver.PASS_NAME[transform_pass]

        profile_file = benchmark.get_profile()
        traces = benchmark.get_traces()
        tdgs = benchmark.get_tdgs(transform_pass)

        assert(len(traces) == len(tdgs))
        if name not in self.transform_jobs:
            self.transform_jobs[name] = dict()
        assert(transform_pass not in self.transform_jobs[name])
        self.transform_jobs[name][transform_pass] = dict()

        for i in xrange(0, len(traces)):
            if self.options.trace_id:
                if i not in self.options.trace_id:
                    # Ignore those traces if not specified
                    continue
            deps = list()
            if name in self.trace_jobs:
                deps.append(self.trace_jobs[name])

            # Schedule the job.
            self.transform_jobs[name][transform_pass][i] = (
                job_scheduler.add_job(
                    '{name}.{transform_pass}.transform'.format(
                        name=name,
                        transform_pass=transform_pass
                    ),
                    transform,
                    (
                        benchmark,
                        pass_name,
                        traces[i],
                        profile_file,
                        tdgs[i],
                        debugs,
                    ),
                    deps
                )
            )

    def schedule_simulate(self, job_scheduler, benchmark, transform_pass, debugs):
        name = benchmark.get_name()
        tdgs = benchmark.get_tdgs(transform_pass)

        for i in xrange(0, len(tdgs)):
            if self.options.trace_id:
                if i not in self.options.trace_id:
                    # Ignore those traces if not specified
                    continue
            deps = list()
            if name in self.transform_jobs:
                if transform_pass in self.transform_jobs[name]:
                    deps.append(self.transform_jobs[name][transform_pass][i])

            gem5_configs = self.gem5_config_manager.get_configs(
                transform_pass
            )
            for gem5_config in gem5_configs:
                job_scheduler.add_job(
                    '{name}.{transform_pass}.simulate'.format(
                        name=name,
                        transform_pass=transform_pass,
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

    def simulate_hoffman2(self, benchmarks):
        hoffman2_commands = list()
        retrive_commands = list()
        for benchmark in benchmarks:
            for transform_pass in options.transform_passes:
                name = benchmark.get_name()
                tdgs = benchmark.get_tdgs(transform_pass)
                results = benchmark.get_results(transform_pass)

                for i in xrange(0, len(tdgs)):
                    if self.options.trace_id:
                        if i not in self.options.trace_id:
                            # Ignore those traces if not specified.
                            continue
                    hoffman2_command = benchmark.simulate_hoffman2(
                        tdgs[i], results[i], False)
                    hoffman2_commands.append(hoffman2_command)
                    retrive_command = benchmark.get_hoffman2_retrive_cmd(
                        tdgs[i], results[i])
                    retrive_commands.append(retrive_command)
        for i in xrange(len(hoffman2_commands)):
            print('{i} {cmd}'.format(i=i, cmd=' '.join(hoffman2_commands[i])))
            # Hoffman2 index starts from 1.
            command_file = 'command.{i}.sh'.format(i=i+1)
            hoffman2_command_file = os.path.join(
                C.HOFFMAN2_SSH_SCRATCH, command_file)
            tmp_command_file = os.path.join('/tmp', command_file)
            with open(tmp_command_file, 'w') as f:
                for j in xrange(len(hoffman2_commands[i])):
                    if ' ' in hoffman2_commands[i][j]:
                        idx = hoffman2_commands[i][j].find('=')
                        hoffman2_commands[i][j] = (
                            hoffman2_commands[i][j][0:idx+1] + '"' +
                            hoffman2_commands[i][j][idx+1:-1] + '"'
                        )

                f.write(' '.join(hoffman2_commands[i]))
            print('# SCP command file {i}'.format(i=i+1))
            Util.call_helper([
                'scp',
                tmp_command_file,
                hoffman2_command_file,
            ])
        tmp_retrive_command_file = os.path.join('/tmp', 'retrive_hoffman2.sh')
        with open(tmp_retrive_command_file, 'w') as f:
            for cmd in retrive_commands:
                f.write(' '.join(cmd))
                f.write('\n')


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
    'adfa': [],
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
            for transform_pass in options.transform_passes:
                driver.schedule_simulate(
                    job_scheduler,
                    benchmark,
                    transform_pass,
                    simulate_datagraph_debugs[transform_pass])
    job_scheduler.run()

    # If use hoffman2, prepare the cluster.
    if options.simulate and options.hoffman2:
        driver.simulate_hoffman2(benchmarks)

    suite_result = BenchmarkResult.SuiteResult(
        benchmarks, driver.gem5_config_manager, options.transform_passes)
    energy_attribute = BenchmarkResult.BenchmarkResult.get_attribute_energy()
    time_attribute = BenchmarkResult.BenchmarkResult.get_attribute_time()
    suite_result.compare([energy_attribute, time_attribute])

    if len(options.transform_passes) > 1 and 'replay' in options.transform_passes:
        for transform in options.transform_passes:
            if transform == 'replay':
                continue
            suite_result.compare_transform_speedup(transform)
            suite_result.compare_transform_energy(transform)

    benchmark_stream_statistics = dict()

    for benchmark in benchmarks:

        stream_passes = ['stream', 'stream-prefetch']
        stream_pass_specified = None
        stream_tdgs = list()
        replay_results = list()
        stream_results = list()
        for p in stream_passes:
            if p in options.transform_passes:
                stream_pass_specified = p
                stream_tdgs = benchmark.get_tdgs(p)
                replay_results = benchmark.get_results('replay')
                stream_results = benchmark.get_results(p)
                break

        if stream_pass_specified is not None:
            filtered_trace_ids = list()
            for i in xrange(len(stream_tdgs)):
                if options.trace_id:
                    if i not in options.trace_id:
                        # Ignore those traces if not specified
                        continue
                # Hack here to skip some traces.
                if 'leela_s' in benchmark.get_name():
                    if i in {2, 4, 5, 9}:
                        continue
                filtered_trace_ids.append(i)
            filtered_stream_tdg_stats = [
                stream_tdgs[x] + '.stats.txt' for x in filtered_trace_ids]
            filtered_replay_results = [replay_results[x]
                                       for x in filtered_trace_ids]
            filtered_stream_results = [stream_results[x]
                                       for x in filtered_trace_ids]

            stream_stats = StreamStatistics.StreamStatistics(
                benchmark.get_name(),
                filtered_stream_tdg_stats,
                filtered_replay_results,
                filtered_stream_results)
            benchmark_stream_statistics[benchmark.get_name()] = stream_stats
            print('-------------------------- ' + benchmark.get_name())

    if benchmark_stream_statistics:
     #    StreamStatistics.StreamStatistics.print_benchmark_stream_breakdown(
     #        benchmark_stream_statistics)
     #    StreamStatistics.StreamStatistics.print_benchmark_stream_breakdown_coarse(
     #        benchmark_stream_statistics)
     #    StreamStatistics.StreamStatistics.print_benchmark_stream_breakdown_indirect(
     #        benchmark_stream_statistics)
     #    StreamStatistics.StreamStatistics.print_benchmark_stream_paths(
     #        benchmark_stream_statistics)
        StreamStatistics.StreamStatistics.print_benchmark_chosen_stream_percentage(
            benchmark_stream_statistics)
     #    StreamStatistics.StreamStatistics.print_benchmark_chosen_stream_length(
     #        benchmark_stream_statistics)
     #    StreamStatistics.StreamStatistics.print_benchmark_chosen_stream_indirect(
     #        benchmark_stream_statistics)
     #    StreamStatistics.StreamStatistics.print_benchmark_chosen_stream_loop_path(
     #        benchmark_stream_statistics)
     #    StreamStatistics.StreamStatistics.print_benchmark_chosen_stream_configure_level(
     #        benchmark_stream_statistics)
     #    StreamStatistics.StreamStatistics.print_benchmark_stream_simulation_result(
     #        benchmark_stream_statistics)
        pass


def parse_benchmarks(option, opt, value, parser):
    setattr(parser.values, option.dest, value.split(','))


def parse_stream_engine_maximum_run_ahead_length(option, opt, value, parser):
    vs = value.split(',')
    setattr(parser.values, option.dest, [int(x) for x in vs])


if __name__ == '__main__':
    import optparse
    parser = optparse.OptionParser()
    parser.add_option('--cores', action='store',
                      type='int', dest='cores', default=8)
    parser.add_option('-b', '--build', action='store_true',
                      dest='build', default=False)
    parser.add_option('-t', '--trace', action='store_true',
                      dest='trace', default=False)
    parser.add_option('--directory', action='store',
                      type='string', dest='directory')
    parser.add_option('-p', '--pass', action='append',
                      type='string', dest='transform_passes')
    parser.add_option('--trace-id', action='append', type='int',
                      dest='trace_id')
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
    parser.add_option('--se-ahead', action='callback', type='string',
                      callback=parse_stream_engine_maximum_run_ahead_length, dest='se_ahead')
    parser.add_option('--se-throttling', action='store',
                      type='string', dest='se_throttling', default='static')
    parser.add_option('--se-coalesce', action='store',
                      type='string', dest='se_coalesce', default='single')
    # Enable stream aware cache.
    parser.add_option('--se-l1d', action='store',
                      type='string', dest='se_l1d', default='original')
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
