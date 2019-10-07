import numpy
from Utils.CSVWriter import CSVWriter

"""
Simply prints the speedup.
"""


class SpeedupExperiments(object):
    def __init__(self, driver):
        self.driver = driver

        def float_formatter(x): return '{x:.2f}'.format(x=x)
        numpy.set_printoptions(formatter={'float_kind': float_formatter})

        for transform_config in self.driver.transform_manager.get_all_configs():
            transform_id = transform_config.get_transform_id()
            for simulation_config in self.driver.simulation_manager.get_configs(transform_id):
                print(simulation_config.get_simulation_id())

        for benchmark in self.driver.benchmarks:
            simulation_times = numpy.zeros(0)
            for transform_config in self.driver.transform_manager.get_all_configs():
                transform_id = transform_config.get_transform_id()
                for simulation_config in self.driver.simulation_manager.get_configs(transform_id):
                    time = self.get_weighted_simulation_time(
                        benchmark, transform_config, simulation_config)
                    simulation_times = numpy.append(simulation_times, time)
            # Divide by the first one?
            speedup = numpy.reciprocal(numpy.divide(
                simulation_times, simulation_times[0]))
            print('{b:40} {spd}'.format(b=benchmark.get_name(), spd=speedup))

    def get_weighted_simulation_time(self, benchmark, transform_config, simulation_config):
        time = 0.0
        for trace in benchmark.get_traces():
            result = self.driver.get_simulation_result(
                benchmark, trace, transform_config, simulation_config
            )
            time += trace.weight * result.stats.get_sim_seconds()
        return time


def analyze(driver):
    SpeedupExperiments(driver)
