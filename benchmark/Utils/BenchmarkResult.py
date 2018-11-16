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
    def __init__(self, benchmark, gem5_config_manager, transforms):
        self.benchmark = benchmark
        self.gem5_config_manager = gem5_config_manager
        self.transform_results = dict()
        for transform in transforms:
            self.transform_results[transform] = list()

            tdgs = self.benchmark.get_tdgs(transform)
            gem5_configs = self.gem5_config_manager.get_configs(transform)

            for gem5_config in gem5_configs:
                folders = [gem5_config.get_gem5_dir(
                    tdg) for tdg in tdgs]
                self.transform_results[transform].append(
                    TransformResult(benchmark, folders)
                )

    def compute(self, func, transforms):
        values = list()
        for transform in transforms:
            assert(transform in self.transform_results)
            # Compute function only calculate for the first
            # result.
            result = self.transform_results[transform][0]
            values.append(func(result))
        return values

    def compute_speedup_over_replay(self, transform):
        replay_time = self.transform_results['replay'][0].compute_time()
        speedups = list()
        for transform_result in self.transform_results[transform]:
            transform_time = transform_result.compute_time()
            speedups.append(replay_time / transform_time)
        return speedups

    def compute_energy_efficiency_over_replay(self, transform):
        replay_energy = self.transform_results['replay'][0].compute_energy()
        efficiency = list()
        for transform_result in self.transform_results[transform]:
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
    def __init__(self, benchmarks, gem5_config_manager, transforms):
        self.transforms = transforms
        self.benchmark_results = dict()
        self.gem5_config_manager = gem5_config_manager
        for benchmark in benchmarks:
            result = BenchmarkResult(
                benchmark, gem5_config_manager, transforms)
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
                attribute_names, self.transforms))
        for benchmark in self.benchmark_results:
            result = self.benchmark_results[benchmark]
            data = list()
            for attribute in attributes:
                attribute_values = attribute(result, self.transforms)
                data += attribute_values
            table.add_row(benchmark.get_name(), data)
        # If there are two transforms, compare the attributes.
        if len(self.transforms) == 2:
            for b in xrange(len(attributes)):
                table.add_ratio_column(b * 2, b * 2 + 1)
        # Add one more geomean for speedup.
        for a in xrange(len(attributes)):
            if len(self.transforms) != 2:
                break
            if attributes[a].name == 'time':
                table.add_geomean_row(
                    len(self.transforms) * len(attributes) + a)

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

    def compare_transform_speedup(self, transform):
        configs = self.gem5_config_manager.get_configs(transform)
        config_ids = [config.get_config_id(transform) for config in configs]
        table = SimpleTable.SimpleTable(
            'benchmark', config_ids
        )
        for benchmark in self.ordered_benchmarks:
            result = self.benchmark_results[benchmark]
            speedups = result.compute_speedup_over_replay(transform)
            table.add_row(benchmark.get_name(), speedups)
        print(table)
        return table

    """
    Return a table like this
    -------------------------------------------------------------------
    Benchmark   | Energy Eff of Config 1 | Energy Eff of Config 2 | ...
    -------------------------------------------------------------------
    """

    def compare_transform_energy(self, transform):
        configs = self.gem5_config_manager.get_configs(transform)
        config_ids = [config.get_config_id(transform) for config in configs]
        table = SimpleTable.SimpleTable(
            'benchmark', config_ids
        )
        for benchmark in self.ordered_benchmarks:
            result = self.benchmark_results[benchmark]
            speedups = result.compute_energy_efficiency_over_replay(transform)
            table.add_row(benchmark.get_name(), speedups)
        print(table)
        return table
