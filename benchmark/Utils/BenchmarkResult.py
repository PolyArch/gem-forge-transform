import Gem5RegionStats
import Gem5Stats
import McPAT
import SimpleTable

import os
import json
import itertools


class Attribute:
    def __init__(self, name, func):
        self.name = name
        self.func = func

    def __call__(self, benchmark_result, transforms):
        return self.func(benchmark_result, transforms)


class TransformResult:
    def __init__(self, benchmark, folders):
        self.benchmark = benchmark
        self.folders = folders
        self.region_stats = list()
        self.stats = list()
        self.mcpats = list()
        self.config = None
        for folder in folders:
            config = os.path.join(folder, 'config.json')
            with open(config) as f:
                self.config = json.load(f)
            stats = os.path.join(folder, 'stats.txt')
            self.stats.append(Gem5Stats.Gem5Stats(benchmark, stats))
            region_stats = os.path.join(folder, 'region.stats.txt')
            self.region_stats.append(
                Gem5RegionStats.Gem5RegionStats(benchmark, region_stats))
            mcpat = os.path.join(folder, 'mcpat.txt')
            self.mcpats.append(McPAT.McPAT(mcpat))

    def compute_energy(self, idx=-1):
        if idx == -1:
            return sum([self.compute_energy(i) for i in xrange(len(self.folders))])

        system_power = self.mcpats[idx].get_system_power()
        time = self.compute_time(idx)
        energy = system_power * time
        return energy

    def compute_time(self, idx=-1):
        if idx == -1:
            return sum([self.compute_time(i) for i in xrange(len(self.folders))])

        time = self.stats[idx].get_sim_seconds()
        return time


class BenchmarkResult:
    def __init__(self, benchmark, transform_manager, gem5_config_manager, transforms):
        self.benchmark = benchmark
        self.transform_manager = transform_manager
        self.gem5_config_manager = gem5_config_manager
        self.transform_results = dict()
        for transform in transforms:
            for transform_config in self.transform_manager.get_configs(transform):
                transform_id = transform_config.get_id()
                self.transform_results[transform_id] = list()

                tdgs = self.benchmark.get_tdgs(transform_config)
                gem5_configs = self.gem5_config_manager.get_configs(transform)

                for gem5_config in gem5_configs:
                    folders = [gem5_config.get_gem5_dir(
                        tdg) for tdg in tdgs]
                    self.transform_results[transform_id].append(
                        TransformResult(benchmark, folders)
                    )

    def compute(self, func, transform_ids):
        values = list()
        for transform_id in transform_ids:
            assert(transform_id in self.transform_results)
            # Compute function only calculate for the first
            # result.
            result = self.transform_results[transform_id][0]
            values.append(func(result))
        return values

    def compute_speedup_over_replay(self, transform_id):
        replay_time = self.transform_results['replay'][0].compute_time()
        speedups = list()
        for transform_result in self.transform_results[transform_id]:
            transform_time = transform_result.compute_time()
            speedups.append(replay_time / transform_time)
        return speedups

    def compute_energy_efficiency_over_replay(self, transform_id):
        replay_energy = self.transform_results['replay'][0].compute_energy()
        efficiency = list()
        for transform_result in self.transform_results[transform_id]:
            transform_energy = transform_result.compute_energy()
            efficiency.append(replay_energy / transform_energy)
        return efficiency

    @staticmethod
    def get_attribute_energy():
        return Attribute('energy', lambda self, transforms: self.compute(TransformResult.compute_energy, transforms))

    @staticmethod
    def get_attribute_time():
        return Attribute('time', lambda self, transforms: self.compute(TransformResult.compute_time, transforms))


class SuiteResult:
    def __init__(self, benchmarks, transform_manager, gem5_config_manager, transforms):
        self.transforms = transforms
        self.transform_ids = list()
        for transform in self.transforms:
            for config in transform_manager.get_configs(transform):
                self.transform_ids.append(config.get_id())
        self.benchmark_results = dict()
        self.transform_manager = transform_manager
        self.gem5_config_manager = gem5_config_manager
        for benchmark in benchmarks:
            result = BenchmarkResult(
                benchmark, transform_manager, gem5_config_manager, transforms)
            self.benchmark_results[benchmark] = result
        self.ordered_benchmarks = list()
        for benchmark in benchmarks:
            self.ordered_benchmarks.append(benchmark)
        self.ordered_benchmarks.sort(key=lambda b: b.get_name())

    """
    Return a table like this
    ---------------------------------------------------------
    Benchmark   | Attribute0                | Attribute1 ...
                | Transform0 Transform1 ... | Transform0 ...
    ---------------------------------------------------------
    The attribute must a lambda that (benchmark_result, transforms) -> data
    """

    def compare(self, attributes):
        attribute_names = [a.name for a in attributes]
        table = SimpleTable.SimpleTable(
            'benchmark', SuiteResult._cross_product(
                attribute_names, self.transform_ids))
        for benchmark in self.benchmark_results:
            result = self.benchmark_results[benchmark]
            data = list()
            for attribute in attributes:
                attribute_values = attribute(result, self.transform_ids)
                data += attribute_values
            table.add_row(benchmark.get_name(), data)
        # If there are two transforms, compare the attributes.
        if len(self.transform_ids) == 2:
            for b in xrange(len(attributes)):
                table.add_ratio_column(b * 2, b * 2 + 1)
        # Add one more geomean for speedup.
        for a in xrange(len(attributes)):
            if len(self.transform_ids) != 2:
                break
            if attributes[a].name == 'time':
                table.add_geomean_row(
                    len(self.transform_ids) * len(attributes) + a)
            if attributes[a].name == 'energy':
                table.add_geomean_row(
                    len(self.transform_ids) * len(attributes) + a)

        print(table)
        return table

    @staticmethod
    def _cross_product(lhs, rhs):
        return map(lambda t: '{x}.{y}'.format(x=t[0], y=t[1]), itertools.product(lhs, rhs))

    """
    Return a table like this
    -------------------------------------------------------------
    Benchmark   | Speedup of Config 1 | Speedup of Config 2 | ...
    -------------------------------------------------------------
    """

    def compare_transform_speedup(self, transform_config):
        transform = transform_config.get_transform()
        transform_id = transform_config.get_id()
        configs = self.gem5_config_manager.get_configs(transform)
        config_ids = [config.get_config_id(transform_id) for config in configs]
        table = SimpleTable.SimpleTable(
            'benchmark', config_ids
        )
        for benchmark in self.ordered_benchmarks:
            result = self.benchmark_results[benchmark]
            speedups = result.compute_speedup_over_replay(transform_id)
            table.add_row(benchmark.get_name(), speedups)
        print(table)
        return table

    """
    Return a table like this
    -------------------------------------------------------------------
    Benchmark   | Energy Eff of Config 1 | Energy Eff of Config 2 | ...
    -------------------------------------------------------------------
    """

    def compare_transform_energy(self, transform_config):
        transform = transform_config.get_transform()
        transform_id = transform_config.get_id()
        configs = self.gem5_config_manager.get_configs(transform)
        config_ids = [config.get_config_id(transform_id) for config in configs]
        table = SimpleTable.SimpleTable(
            'benchmark', config_ids
        )
        for benchmark in self.ordered_benchmarks:
            result = self.benchmark_results[benchmark]
            speedups = result.compute_energy_efficiency_over_replay(
                transform_id)
            table.add_row(benchmark.get_name(), speedups)
        print(table)
        return table
