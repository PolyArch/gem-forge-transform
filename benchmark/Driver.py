import SPEC2017
import MachSuite
import Util
import StreamStatistics
import SPU
import Constants as C

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


def simulate(benchmark, tdg, result, debugs):
    benchmark.simulate(tdg, result, debugs)


class Driver:

    PASS_NAME = {
        'replay': 'replay',
        'adfa': 'abs-data-flow-acc-pass',
        'stream': 'stream-pass',
        'simd': 'simd-pass',
    }

    def __init__(self):
        self.trace_jobs = dict()
        self.transform_jobs = dict()

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
        self.transform_jobs[name][transform_pass] = list()

        for i in xrange(0, len(traces)):
            deps = list()
            if name in self.trace_jobs:
                deps.append(self.trace_jobs[name])

            # Schedule the job.
            self.transform_jobs[name][transform_pass].append(
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
        results = benchmark.get_results(transform_pass)

        for i in xrange(0, len(tdgs)):
            deps = list()
            if name in self.transform_jobs:
                if transform_pass in self.transform_jobs[name]:
                    assert(len(tdgs) == len(
                        self.transform_jobs[name][transform_pass]))
                    deps.append(self.transform_jobs[name][transform_pass][i])

            job_scheduler.add_job(
                '{name}.{transform_pass}.simulate'.format(
                    name=name,
                    transform_pass=transform_pass,
                ),
                simulate,
                (
                    benchmark,
                    tdgs[i],
                    results[i],
                    debugs,
                ),
                deps
            )


build_datagraph_debugs = {
    'stream': [
        'ReplayPass',
        # 'StreamPass',
        # 'DataGraph',
        'LoopUtils',
    ],
    'replay': [],
    'adfa': [],
    'simd': [],
}


simulate_datagraph_debugs = {
    'stream': [],
    'replay': [],
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
    else:
        print('Unknown suite ' + options.suite)
        assert(False)


def main(options):
    job_scheduler = Util.JobScheduler(8, 1)
    test_suite = choose_suite(options)
    benchmarks = test_suite.get_benchmarks()
    driver = Driver()
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
        if options.simulate:
            for transform_pass in options.transform_passes:
                driver.schedule_simulate(
                    job_scheduler,
                    benchmark,
                    transform_pass,
                    simulate_datagraph_debugs[transform_pass])
    job_scheduler.run()

    for benchmark in benchmarks:
        tdgs = benchmark.get_tdgs('stream')
        tdg_stats = [tdg + '.stats.txt' for tdg in tdgs]
        stream_stats = StreamStatistics.StreamStatistics(tdg_stats)
        print('-------------------------- ' + benchmark.get_name())
        stream_stats.print_stats()
        stream_stats.print_access()
        stream_stats.dump_csv(os.path.join(
            C.LLVM_TDG_RESULT_DIR,
            benchmark.get_name() + '.stream.stats.csv'
        ))

    # Util.ADFAAnalyzer.SYS_CPU_PREFIX = 'system.cpu.'
    # Util.ADFAAnalyzer.analyze_adfa(benchmarks)


if __name__ == '__main__':
    import optparse
    parser = optparse.OptionParser()
    parser.add_option('-b', '--build', action='store_true',
                      dest='build', default=False)
    parser.add_option('-t', '--trace', action='store_true',
                      dest='trace', default=False)
    parser.add_option('--directory', action='store',
                      type='string', dest='directory')
    parser.add_option('-p', '--pass', action='append',
                      type='string', dest='transform_passes')
    parser.add_option('-d', '--build-datagraph', action='store_true',
                      dest='build_datagraph', default=False)
    parser.add_option('-s', '--simulate', action='store_true',
                      dest='simulate', default=False)
    parser.add_option('--suite', action='store', type='string', dest='suite')
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
