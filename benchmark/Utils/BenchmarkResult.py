import Gem5RegionStats
import Gem5Stats
import StreamEngineEnergy
import HWPrefetchEnergy
import McPAT
import SimpleTable

import os
import json
import itertools


class SimulationResult:
    def __init__(self, benchmark, trace, transform_config, simulation_config):
        self.benchmark = benchmark
        self.trace = trace
        self.transform_config = transform_config
        self.simulation_config = simulation_config

        self.tdg = self.benchmark.get_tdg(self.transform_config, self.trace)
        self.result_folder = self.simulation_config.get_gem5_dir(self.tdg)

        # Load config.json.
        config_fn = os.path.join(self.result_folder, 'config.json')
        with open(config_fn, 'r') as f:
            self.config = json.load(f)

        # Load stats.
        stats_fn = os.path.join(self.result_folder, 'stats.txt')
        self.stats = Gem5Stats.Gem5Stats(self.benchmark, stats_fn)

        # Load region stats.
        region_stats_fn = os.path.join(self.result_folder, 'region.stats.txt')
        self.region_stats = Gem5RegionStats.Gem5RegionStats(
            self.benchmark, region_stats_fn)

        # Load mcpat.
        mcpat_fn = os.path.join(self.result_folder, 'mcpat.txt')
        self.mcpat = McPAT.McPAT(mcpat_fn)

        # Load stream engine energy.
        self.se_energy = StreamEngineEnergy.StreamEngineEnergy(self.stats)


class Attribute:
    def __init__(self, name, func):
        self.name = name
        self.func = func

    def __call__(self, benchmark_result, transforms):
        return self.func(benchmark_result, transforms)


class TransformResult:
    def __init__(self, benchmark, folders, config_id):
        self.benchmark = benchmark
        self.folders = folders
        self.config_id = config_id
        self.region_stats = list()
        self.stats = list()
        self.mcpats = list()
        self.ses = list()
        self.config = None
        for folder in folders:
            config = os.path.join(folder, 'config.json')
            with open(config) as f:
                self.config = json.load(f)
            stats = os.path.join(folder, 'stats.txt')
            self.stats.append(Gem5Stats.Gem5Stats(benchmark, stats))
            self.ses.append(
                StreamEngineEnergy.StreamEngineEnergy(self.stats[-1]))
            region_stats = os.path.join(folder, 'region.stats.txt')
            self.region_stats.append(
                Gem5RegionStats.Gem5RegionStats(benchmark, region_stats))
            mcpat = os.path.join(folder, 'mcpat.txt')
            self.mcpats.append(McPAT.McPAT(mcpat))
        self.merged_region_stats = Gem5RegionStats.Gem5RegionStats(
            benchmark, [os.path.join(folder, 'region.stats.txt') for folder in folders])

    def compute_energy(self, idx=-1):
        if idx == -1:
            return sum([self.compute_energy(i) for i in xrange(len(self.folders))])

        system_power = self.mcpats[idx].get_system_power()
        time = self.compute_time(idx)
        energy = system_power * time
        return energy

    def compute_se_energy(self, idx=-1):
        if idx == -1:
            return sum([self.compute_se_energy(i) for i in xrange(len(self.folders))])
        return self.ses[idx].get_se_energy()

    def compute_hw_prefetch_energy(self, idx=-1):
        # Initialize the hw prefetcher energy.
        if not hasattr(self, 'hw_prefetches'):
            self.hw_prefetches = [
                HWPrefetchEnergy.HWPrefetchEnergy(s) for s in self.stats]
        return sum([h.get_hw_prefetch_energy() for h in self.hw_prefetches])

    def compute_time(self, idx=-1):
        if idx == -1:
            return sum([self.compute_time(i) for i in xrange(len(self.folders))])

        time = self.stats[idx].get_sim_seconds()
        return time

    def compute_placed_level(self, level, idx=-1):
        if idx == -1:
            return sum([self.compute_placed_level(level, i) for i in xrange(len(self.folders))])
        accesses = self.stats[idx].__getitem__(
            'tdg.accs.stream.numAccessPlacedInCacheLevel::{level}'.format(level=level))
        return accesses

    def compute_hit_higher(self, level, idx=-1):
        if idx == -1:
            return sum([self.compute_hit_higher(level, i) for i in xrange(len(self.folders))])
        accesses = self.stats[idx].__getitem__(
            'tdg.accs.stream.numAccessHitHigherThanPlacedCacheLevel::{level}'.format(level=level))
        return accesses

    def compute_hit_lower(self, level, idx=-1):
        if idx == -1:
            return sum([self.compute_hit_lower(level, i) for i in xrange(len(self.folders))])
        accesses = self.stats[idx].__getitem__(
            'tdg.accs.stream.numAccessHitLowerThanPlacedCacheLevel::{level}'.format(level=level))
        return accesses

    def compute_l1d_hits(self, idx=-1):
        if idx == -1:
            return sum([self.compute_l1d_hits(i) for i in xrange(len(self.folders))])
        l1d_hits = self.stats[idx].__getitem__(
            'system.cpu.dcache.demand_hits::total')
        return l1d_hits

    def compute_l1d_misses(self, idx=-1):
        if idx == -1:
            return sum([self.compute_l1d_misses(i) for i in xrange(len(self.folders))])
        l1d_misses = self.stats[idx].__getitem__(
            'system.cpu.dcache.demand_misses::total')
        return l1d_misses

    def compute_l1_5d_hits(self, idx=-1):
        if idx == -1:
            return sum([self.compute_l1_5d_hits(i) for i in xrange(len(self.folders))])
        l1d_hits = self.stats[idx].get_default(
            'system.cpu.l1_5dcache.demand_hits::total', 0)
        return l1d_hits

    def compute_l1_5d_misses(self, idx=-1):
        if idx == -1:
            return sum([self.compute_l1_5d_misses(i) for i in xrange(len(self.folders))])
        l1d_misses = self.stats[idx].get_default(
            'system.cpu.l1_5dcache.demand_misses::total', 0)
        return l1d_misses

    def compute_l1d_coalesce_hits(self, idx=-1):
        if idx == -1:
            return sum([self.compute_l1d_coalesce_hits(i) for i in xrange(len(self.folders))])
        l1d_hits = self.stats[idx].get_default(
            'system.cpu.dcache.coalesced_demand_hits::total', 0)
        return l1d_hits

    def compute_l1d_coalesce_misses(self, idx=-1):
        if idx == -1:
            return sum([self.compute_l1d_coalesce_misses(i) for i in xrange(len(self.folders))])
        l1d_misses = self.stats[idx].get_default(
            'system.cpu.dcache.coalesced_demand_misses::total', 0)
        return l1d_misses


class BenchmarkResult:
    def __init__(self, benchmark, transform_manager, simulation_manager, transforms):
        self.benchmark = benchmark
        self.transform_manager = transform_manager
        self.simulation_manager = simulation_manager
        self.transform_results = dict()
        for transform in transforms:
            for transform_config in self.transform_manager.get_configs(transform):
                transform_id = transform_config.get_transform_id()
                self.transform_results[transform_id] = list()

                tdgs = self.benchmark.get_tdgs(transform_config)
                gem5_configs = self.simulation_manager.get_configs(transform)

                for gem5_config in gem5_configs:
                    folders = [gem5_config.get_gem5_dir(
                        tdg) for tdg in tdgs]
                    config_id = gem5_config.get_config_id(transform, '')
                    self.transform_results[transform_id].append(
                        TransformResult(benchmark, folders, config_id)
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

    def compute_hit_lower(self, transform_id):
        transform_result = self.transform_results[transform_id][0]
        return [transform_result.compute_hit_lower(level) for level in xrange(5)]

    def compute_hit_higher(self, transform_id):
        transform_result = self.transform_results[transform_id][0]
        return [transform_result.compute_hit_higher(level) for level in xrange(5)]

    def compute_stream_placement(self, transform_id):
        transform_result = self.transform_results[transform_id][0]
        return [transform_result.compute_placed_level(level) for level in xrange(5)]

    def compute_cache_hit_rate(self, transform_id):
        transform_result = self.transform_results[transform_id][0]
        hit_rates = list()
        l1d_hits = transform_result.compute_l1d_hits()
        l1d_misses = transform_result.compute_l1d_misses()
        hit_rates.append(l1d_hits / (l1d_hits + l1d_misses))
        l1_5d_hits = transform_result.compute_l1_5d_hits()
        l1_5d_misses = transform_result.compute_l1_5d_misses()
        hit_rates.append(l1_5d_hits / (l1_5d_hits + l1_5d_misses))
        return hit_rates

    def compute_cache_coalesce_hit_rate(self, transform_id):
        transform_result = self.transform_results[transform_id][0]
        hit_rates = list()
        l1d_hits = transform_result.compute_l1d_coalesce_hits()
        l1d_misses = transform_result.compute_l1d_coalesce_misses()
        hit_rates.append(l1d_hits / (l1d_hits + l1d_misses))
        return hit_rates

    @staticmethod
    def get_attribute_energy():
        return Attribute('energy', lambda self, transforms: self.compute(TransformResult.compute_energy, transforms))

    @staticmethod
    def get_attribute_se_energy():
        return Attribute('se_energy', lambda self, transforms: self.compute(TransformResult.compute_se_energy, transforms))

    @staticmethod
    def get_attribute_time():
        return Attribute('time', lambda self, transforms: self.compute(TransformResult.compute_time, transforms))


class SuiteResult:
    def __init__(self, suite, benchmarks, transform_manager, simulation_manager, transforms):
        self.suite = suite
        self.transforms = transforms
        self.transform_ids = list()
        for transform in self.transforms:
            for config in transform_manager.get_configs(transform):
                self.transform_ids.append(config.get_transform_id())
        self.benchmark_results = dict()
        self.transform_manager = transform_manager
        self.simulation_manager = simulation_manager
        for benchmark in benchmarks:
            result = BenchmarkResult(
                benchmark, transform_manager, simulation_manager, transforms)
            self.benchmark_results[benchmark] = result
        self.ordered_benchmarks = list()
        for benchmark in benchmarks:
            self.ordered_benchmarks.append(benchmark)
        self.ordered_benchmarks.sort(key=lambda b: b.get_name())

    def pickle(self, folder):
        # Only pickle myself when I have one transformation and simulation.
        assert(len(self.transforms) == 1)
        transform = self.transforms[0]
        transform_configs = self.transform_manager.get_configs(transform)
        assert(len(transform_configs) == 1)
        transform_id = transform_configs[0].get_transform_id()
        gem5_configs = self.simulation_manager.get_configs(transform)
        assert(len(gem5_configs) == 1)
        gem5_config_id = gem5_configs[0].get_config_id(transform, prefix='')
        fn = '{suite}.{transform_id}{gem5_config_id}.results.dat'.format(
            suite=self.suite,
            transform_id=transform_id,
            gem5_config_id=gem5_config_id,
        )
        fn = os.path.join(folder, fn)
        print fn
        import pickle
        with open(fn, mode='wb') as f:
            pickle.dump(self, f)

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
        transform_id = transform_config.get_transform_id()
        configs = self.simulation_manager.get_configs(transform)
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
        transform_id = transform_config.get_transform_id()
        configs = self.simulation_manager.get_configs(transform)
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

    def show_region_stats(self):
        regions_aggregated = dict()
        for benchmark in self.ordered_benchmarks:
            result = self.benchmark_results[benchmark]
            if not 'replay' in result.transform_results:
                continue
            assert('replay' in result.transform_results)
            replay_result = result.transform_results['replay'][0]
            region_stats = replay_result.merged_region_stats
            for region in region_stats.regions:
                uid = '{b}.{r}'.format(b=benchmark.get_name(), r=region)
                regions_aggregated[uid] = dict()
                stats = region_stats.regions[region]
                aggr_stats = regions_aggregated[uid]
                aggr_stats['benchmark'] = benchmark.get_name()
                aggr_stats['region'] = region
                aggr_stats['enter'] = stats.get_default('region.entered', 1)
                aggr_stats['insts'] = stats.get_cpu_insts()
                aggr_stats['time'] = stats.get_sim_seconds()
                aggr_stats['mem_acc'] = \
                    stats.get_default('system.cpu.dcache.overall_accesses', 0) / \
                    aggr_stats['insts'] * 1e6
                aggr_stats['l1_m'] = stats.get_default('system.cpu.dcache.overall_misses', 0) / \
                    aggr_stats['insts'] * 1e6
                aggr_stats['l2_m'] = stats.get_default('system.l2.overall_misses', 0) / \
                    aggr_stats['insts'] * 1e6
                aggr_stats['br'] = stats.get_branches() / \
                    aggr_stats['insts'] * 1e6
                aggr_stats['br_m'] = stats.get_branch_misses() / \
                    aggr_stats['insts'] * 1e6
        title = [
            'benchmark',
            'region',
            'enter',
            'insts',
            'time',
            'mem_acc',
            'l1_m',
            'l2_m',
            'br',
            'br_m',
        ]

        special_id = 'haha'
        for benchmark in self.ordered_benchmarks:
            result = self.benchmark_results[benchmark]
            for transform in result.transform_results:
                if transform == 'replay':
                    continue
                for transform_result in result.transform_results[transform]:
                    transform_time = '{t}_time'.format(
                        t=transform_result.config_id)
                    special_id = transform_result.config_id
                    if transform_time not in title:
                        title.append(transform_time)
                    merged_region_stats = transform_result.merged_region_stats
                    for region in merged_region_stats.regions:
                        region_stats = merged_region_stats.regions[region]
                        uid = '{b}.{r}'.format(
                            b=benchmark.get_name(), r=region)
                        if uid not in regions_aggregated:
                            regions_aggregated[uid] = dict()
                        aggr_stats = regions_aggregated[uid]
                        aggr_stats['benchmark'] = benchmark.get_name()
                        aggr_stats['region'] = region
                        aggr_stats[transform_time] = region_stats.get_sim_seconds()

        dump_fn = '{s}.{sp}.regions.csv'.format(s=self.suite, sp=special_id)
        with open(dump_fn, 'w') as f:
            f.write(','.join(title))
            f.write('\n')
            for uid in regions_aggregated:
                aggr_stats = regions_aggregated[uid]
                for t in title:
                    if not t in aggr_stats:
                        aggr_stats[t] = -1
                    f.write('{v},'.format(v=aggr_stats[t]))
                f.write('\n')

    def show_hit_lower(self, transform_config):
        transform = transform_config.get_transform()
        transform_id = transform_config.get_transform_id()
        config = self.simulation_manager.get_configs(transform)[0]
        table = SimpleTable.SimpleTable(
            'benchmark', [str(x) for x in xrange(5)])
        for benchmark in self.ordered_benchmarks:
            result = self.benchmark_results[benchmark]
            hit_lower_row = result.compute_hit_lower(transform_id)
            table.add_row(benchmark.get_name(), hit_lower_row)
        print("hit lower table")
        print(table)

    def show_hit_higher(self, transform_config):
        transform = transform_config.get_transform()
        transform_id = transform_config.get_transform_id()
        config = self.simulation_manager.get_configs(transform)[0]
        table = SimpleTable.SimpleTable(
            'benchmark', [str(x) for x in xrange(5)])
        for benchmark in self.ordered_benchmarks:
            result = self.benchmark_results[benchmark]
            hit_lower_row = result.compute_hit_higher(transform_id)
            table.add_row(benchmark.get_name(), hit_lower_row)
        print("hit higher table")
        print(table)

    def show_stream_placement(self, transform_config):
        transform = transform_config.get_transform()
        transform_id = transform_config.get_transform_id()
        config = self.simulation_manager.get_configs(transform)[0]
        table = SimpleTable.SimpleTable(
            'benchmark', [str(x) for x in xrange(5)])
        for benchmark in self.ordered_benchmarks:
            result = self.benchmark_results[benchmark]
            hit_lower_row = result.compute_stream_placement(transform_id)
            table.add_row(benchmark.get_name(), hit_lower_row)
        print("stream placed level table")
        print(table)

    def show_cache_hits(self, transform_config):
        transform = transform_config.get_transform()
        transform_id = transform_config.get_transform_id()
        config = self.simulation_manager.get_configs(transform)[0]
        table = SimpleTable.SimpleTable('benchmark', ['l1d_hit', 'l1_5d_hit'])
        for benchmark in self.ordered_benchmarks:
            result = self.benchmark_results[benchmark]
            cache_hit_rates = result.compute_cache_hit_rate(transform_id)
            table.add_row(benchmark.get_name(), cache_hit_rates)
        print('cache hit table')
        print(table)

    def show_cache_coalesce_hits(self, transform_config):
        transform = transform_config.get_transform()
        transform_id = transform_config.get_transform_id()
        config = self.simulation_manager.get_configs(transform)[0]
        table = SimpleTable.SimpleTable('benchmark', ['l1d_hit'])
        for benchmark in self.ordered_benchmarks:
            result = self.benchmark_results[benchmark]
            cache_hit_rates = result.compute_cache_coalesce_hit_rate(
                transform_id)
            table.add_row(benchmark.get_name(), cache_hit_rates)
        print('cache coalesce hit table')
        print(table)
