
from BenchmarkDrivers.MultiProgramBenchmark import MultiProgramBenchmark

import Utils.Gem5Stats as Gem5Stats

import os


class StreamExperiments(object):
    def __init__(self, driver):
        self.driver = driver
        self.stream_transform_config = self.driver.transform_manager.get_config(
            'stream')
        self.replay_transform_config = self.driver.transform_manager.get_config(
            'replay')

        self.stream_simulation_configs = self.driver.simulation_manager.get_configs(
            'stream')
        self.replay_simulation_configs = self.driver.simulation_manager.get_configs(
            'replay')

        self.single_program_speedup = dict()
        self.multi_program_speedup = dict()

        for benchmark in self.driver.benchmarks:
            print('Start to analyze {benchmark}'.format(
                benchmark=benchmark.get_name()))
            if isinstance(benchmark, MultiProgramBenchmark):
                self.analyzeMultiProgramBenchmark(benchmark)
            else:
                speedup = self.analyzeSingleBenchmark(benchmark)

        for replay_simulation_config in self.replay_simulation_configs:
            for stream_simulation_config in self.stream_simulation_configs:
                single_geomean = self.geomean(
                    replay_simulation_config, stream_simulation_config,
                    self.single_program_speedup)
                multi_geomean = self.geomean(
                    replay_simulation_config, stream_simulation_config,
                    self.multi_program_speedup)
                print('geomean single {s} multi {m} diff {d}'.format(
                    s=single_geomean,
                    m=multi_geomean,
                    d=multi_geomean/single_geomean,
                ))

    def geomean(self, replay_simulation_config, stream_simulation_config, spd):
        all_speedup = [spd[b][replay_simulation_config]
                       [stream_simulation_config] for b in spd]
        if len(all_speedup) == 0:
            return 1.0
        prod = 1.0
        for x in all_speedup:
            prod *= x
        print prod
        return prod ** (1./len(all_speedup))

    def get_single_program_time(self, benchmark, transform_config, simulation_config):
        time = 0.0
        for trace in benchmark.get_traces():
            result = self.driver.get_simulation_result(
                benchmark, trace, transform_config, simulation_config)
            time += result.stats.get_sim_seconds() * trace.weight
        return time

    def analyzeSingleBenchmark(self, benchmark):
        replay_time = dict()
        for replay_simulation_config in self.replay_simulation_configs:
            time = self.get_single_program_time(
                benchmark, self.replay_transform_config, replay_simulation_config)
            replay_time[replay_simulation_config] = time
        stream_time = dict()
        for stream_simulation_config in self.stream_simulation_configs:
            time = self.get_single_program_time(
                benchmark, self.stream_transform_config, stream_simulation_config)
            stream_time[stream_simulation_config] = time
        speedups = dict()
        for replay_simulation_config in replay_time:
            speedups[replay_simulation_config] = dict()
            for stream_simulation_config in stream_time:
                speedup = replay_time[replay_simulation_config] / \
                    stream_time[stream_simulation_config]
                speedups[replay_simulation_config][stream_simulation_config] = speedup
                print('{b} Speedup {stream}/{replay} is {speedup}.'.format(
                    b=benchmark.get_name(),
                    stream=stream_simulation_config.get_simulation_id(),
                    replay=replay_simulation_config.get_simulation_id(),
                    speedup=speedup
                ))
        self.single_program_speedup[benchmark] = speedups
        return speedups

    def getAllTimeForMultiProgramBenchmark(self,
                                           multi_program_benchmark, multi_program_tdgs, simulation_configs):

        benchmark_single_time = dict()
        benchmark_multi_time = dict()
        for benchmark in multi_program_benchmark.benchmarks:
            benchmark_single_time[benchmark] = dict()
            benchmark_multi_time[benchmark] = dict()
            for simulation_config in simulation_configs:
                benchmark_single_time[benchmark][simulation_config] = 0.0
                benchmark_multi_time[benchmark][simulation_config] = 0.0

        for t in multi_program_tdgs:
            multi_program_tdg = multi_program_benchmark.tdg_map[t]
            single_program_tdgs = multi_program_tdg.tdgs
            assert(len(single_program_tdgs) == len(
                multi_program_benchmark.benchmarks))

            # Load multi program time.
            for simulation_config in simulation_configs:
                multi_program_tdg_sim_dir = simulation_config.get_gem5_dir(t)
                multi_program_tdg_sim_stats = os.path.join(
                    multi_program_tdg_sim_dir, 'stats.txt')
                gem5Stats = Gem5Stats.Gem5Stats(
                    multi_program_benchmark, multi_program_tdg_sim_stats)
                for i in range(len(single_program_tdgs)):
                    benchmark = multi_program_benchmark.benchmarks[i]
                    time = gem5Stats['system.cpu{i}.numCycles'.format(i=i)]
                    benchmark_multi_time[benchmark][simulation_config] += time

            # Load single program time.
            for i in range(len(single_program_tdgs)):
                benchmark = multi_program_benchmark.benchmarks[i]
                single_program_tdg = single_program_tdgs[i]
                for simulation_config in simulation_configs:
                    single_program_tdg_sim_dir = simulation_config.get_gem5_dir(
                        single_program_tdg)
                    single_program_tdg_sim_stats = os.path.join(
                        single_program_tdg_sim_dir, 'stats.txt')
                    gem5Stats = Gem5Stats.Gem5Stats(
                        benchmark, single_program_tdg_sim_stats)
                    single_program_time = gem5Stats['system.cpu.numCycles']
                    benchmark_single_time[benchmark][simulation_config] += \
                        single_program_time
        return (benchmark_single_time, benchmark_multi_time)

    def speedupHack(self, rp_time, st_time, spd):
        for benchmark in rp_time:
            spd[benchmark] = dict()
            assert(benchmark in st_time)
            for rp_config in self.replay_simulation_configs:
                spd[benchmark][rp_config] = dict()
                assert(rp_config in rp_time[benchmark])
                for st_config in self.stream_simulation_configs:
                    assert(st_config in st_time[benchmark])
                    rp = rp_time[benchmark][rp_config]
                    st = st_time[benchmark][st_config]
                    spd[benchmark][rp_config][st_config] = rp / st

    def analyzeMultiProgramBenchmark(self, multi_program_benchmark):
        # First get all multi-program tdgs.
        replay_multi_program_tdgs = multi_program_benchmark.get_tdgs(
            self.replay_transform_config)
        rp_single, rp_multi = self.getAllTimeForMultiProgramBenchmark(
            multi_program_benchmark, replay_multi_program_tdgs, self.replay_simulation_configs)
        stream_multi_program_tdgs = multi_program_benchmark.get_tdgs(
            self.stream_transform_config)
        st_single, st_multi = self.getAllTimeForMultiProgramBenchmark(
            multi_program_benchmark, stream_multi_program_tdgs, self.stream_simulation_configs)
        # Compute speedup.
        self.speedupHack(rp_single, st_single, self.single_program_speedup)
        self.speedupHack(rp_multi, st_multi, self.multi_program_speedup)

        for rp_config in self.replay_simulation_configs:
            for st_config in self.stream_simulation_configs:
                for benchmark in multi_program_benchmark.benchmarks:
                    single = self.single_program_speedup[benchmark][rp_config][st_config]
                    multi = self.multi_program_speedup[benchmark][rp_config][st_config]
                    print('{b}, single {s}, multi {m}, diff {d}'.format(
                        b=benchmark.get_name(),
                        s=single,
                        m=multi,
                        d=multi/single,
                    ))

        # import pprint
        # pprint.pprint(rp_single)
        # pprint.pprint(rp_multi)
        # pprint.pprint(st_single)
        # pprint.pprint(st_multi)


def analyze(driver):
    StreamExperiments(driver)
