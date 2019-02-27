
from MultiProgramBenchmark import MultiProgramBenchmark


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
                

    def get_time(self, benchmark, transform_config, simulation_config):
        time = 0.0
        for trace in benchmark.get_traces():
            result = self.driver.get_simulation_result(
                benchmark, trace, transform_config, simulation_config)
            time += result.stats.get_sim_seconds()
        return time

    def analyzeSingleBenchmark(self, benchmark):
        replay_time = dict()
        for replay_simulation_config in self.replay_simulation_configs:
            time = self.get_time(
                benchmark, self.replay_transform_config, replay_simulation_config)
            replay_time[replay_simulation_config] = time
        stream_time = dict()
        for stream_simulation_config in self.stream_simulation_configs:
            time = self.get_time(
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

    def analyzeMultiProgramBenchmark(self, multi_program_benchmark):
        for benchmark in multi_program_benchmark.benchmarks:
            # First analyze single benchmarks.
            self.analyzeSingleBenchmark(benchmark)
            # Now analyze the multi-program performance.


        


def analyze(driver):
    StreamExperiments(driver)
