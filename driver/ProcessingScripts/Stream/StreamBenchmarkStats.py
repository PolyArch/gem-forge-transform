from StreamTransformStats import StreamTransformStats

import os


"""
Represent the simulation result of (benchmark, transform_config, simulation_config)
"""


class StreamBenchmarkSimStats(object):
    def __init__(self, driver, benchmark, transform_config, simulation_config):
        self.driver = driver
        self.benchmark = benchmark
        self.transform_config = transform_config
        self.simulation_config = simulation_config

    def weighted_by_trace(self, func):
        value = 0.0
        for trace in self.benchmark.get_traces():
            result = self.driver.get_simulation_result(
                self.benchmark,
                trace,
                self.transform_config,
                self.simulation_config,
            )
            value += func(result) * trace.weight
        return value

    def get_time(self):
        return self.weighted_by_trace(
            lambda x: x.stats.get_sim_seconds()
        )

    def get_energy(self):
        return self.weighted_by_trace(
            lambda x: x.get_energy()
        )

    def dump(self, f, baseline):
        f.write(',{v}'.format(v=baseline.get_time() / self.get_time()))
        f.write(',{v}'.format(v=baseline.get_energy() / self.get_energy()))

    def dump_header(self, f):
        f.write(',time')
        f.write(',energy')

    def num_fields(self):
        fields = 2
        return fields


"""
Represent a single benchmark for the stream experiment.
"""


class StreamBenchmarkStats(object):
    def __init__(self, driver, benchmark):
        self.driver = driver
        self.benchmark = benchmark

        # Get the transform config.
        assert(self.driver.transform_manager.has_config('stream.alias'))
        self.stream_transform_stats = StreamTransformStats(
            self.benchmark, self.driver.transform_manager.get_config('stream.alias'))

        # Reorganize all the simulation configurations.
        self.sim_stats = list()
        transform_ids = [
            'replay',
            # 'stream.prefetch.alias',
            'stream.alias'
        ]
        for transform_id in transform_ids:
            transform_config = self.driver.transform_manager.get_config(
                transform_id)
            simulation_configs = self.driver.simulation_manager.get_configs(
                transform_id)
            simulation_configs.sort(key=lambda x: x.get_simulation_id())
            for simulation_config in simulation_configs:
                simulation_id = simulation_config.get_simulation_id()
                self.sim_stats.append(StreamBenchmarkSimStats(
                    self.driver,
                    self.benchmark,
                    transform_config,
                    simulation_config
                ))
                if transform_id == 'replay' and simulation_id == 'o8.ch2.l3':
                    self.baseline_sim_stats = self.sim_stats[-1]

    def dump(self, f):
        f.write(self.benchmark.get_name())
        sts = self.stream_transform_stats
        f.write(',{v}'.format(
            v=sts.get_total_stream_accesses() / sts.get_total_mem_accesses()))
        f.write(',{v}'.format(
            v=sts.get_total_removed_insts() / sts.get_total_insts()))
        f.write(',{v}'.format(
            v=sts.get_total_added_insts() / sts.get_total_insts()))
        for sbss in self.sim_stats:
            sbss.dump(f, self.baseline_sim_stats)
        f.write('\n')

    def dump_header(self, f):
        f.write('benchmark')
        f.write(',,,')
        for sbss in self.sim_stats:
            f.write(',{tid}-{sid}'.format(
                tid=sbss.transform_config.get_transform_id(),
                sid=sbss.simulation_config.get_simulation_id(),
            ))
            # Write additional fields.
            for i in range(sbss.num_fields() - 1):
                f.write(',')
        f.write('\n')
        f.write(',stream,removed,added')
        for sbss in self.sim_stats:
            sbss.dump_header(f)
        f.write('\n')
