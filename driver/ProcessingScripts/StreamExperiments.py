
from BenchmarkDrivers.MultiProgramBenchmark import MultiProgramBenchmark

import Utils.Gem5Stats as Gem5Stats
import Utils.StreamMessage_pb2 as StreamMessage_pb2
import Utils.SimpleTable as SimpleTable
import json

import os


class StreamExperiments(object):
    def __init__(self, driver):
        self.driver = driver
        stream_transforms = [
            'stream',
            'stream.i',
            'stream.ai',
            'stream.pai',
            'stream.static-outer',
            'stream.alias',
        ]
        self.stream_transform_config = None
        for s in stream_transforms:
            if self.driver.transform_manager.has_config(s):
                self.stream_transform_config = self.driver.transform_manager.get_config(
                    s)

        assert(self.stream_transform_config is not None)
        self.stream_simulation_configs = self.driver.simulation_manager.get_configs(
            self.stream_transform_config.get_transform_id())

        self.analyze_static_stream()

        self.analyze_mismatch_between_chosen_and_static_analysis()
        self.analyze_chosen_stream_length()
        self.analyze_stream_type()
        self.analyze_alias_stream_access()

        """
        Speedup matrix is indexed by Speedup[Benchmark][BaseGemForgeSystemConfig][ExprGemForgeSystemConfig].
        """
        self.single_program_speedup = dict()
        self.multi_program_speedup = dict()

        for benchmark in self.driver.benchmarks:
            if isinstance(benchmark, MultiProgramBenchmark):
                self.analyzeMultiProgramBenchmark(benchmark)
            else:
                speedup_matrix = self.analyze_single_benchmark_speedup_matrix(
                    benchmark)
                self.single_program_speedup[benchmark] = speedup_matrix

        for gem_forge_system_config in self.driver.simulation_manager.get_all_gem_forge_system_configs():
            transform_config = gem_forge_system_config.transform_config
            simulation_config = gem_forge_system_config.simulation_config
            if transform_config.get_transform_id() == 'replay':
                self.print_speedup_over_base_config(
                    self.single_program_speedup, gem_forge_system_config)
            elif simulation_config.get_simulation_id().endswith('o8'):
                self.print_speedup_over_base_config(
                    self.single_program_speedup, gem_forge_system_config)

        # for replay_simulation_config in self.replay_simulation_configs:
        #     for stream_simulation_config in self.stream_simulation_configs:
        #         single_geomean = self.geomean(
        #             replay_simulation_config, stream_simulation_config,
        #             self.single_program_speedup)
        #         multi_geomean = self.geomean(
        #             replay_simulation_config, stream_simulation_config,
        #             self.multi_program_speedup)
        #         print('geomean single {s} multi {m} diff {d}'.format(
        #             s=single_geomean,
        #             m=multi_geomean,
        #             d=multi_geomean/single_geomean,
        #         ))

    def dump(self):
        from Stream import StreamBenchmarkStats
        stream_csv = open('stream.csv', 'w')
        header_dumped = False
        for benchmark in self.driver.benchmarks:
            if isinstance(benchmark, MultiProgramBenchmark):
                # So far ignore this.
                continue
            sbs = StreamBenchmarkStats.StreamBenchmarkStats(
                self.driver, benchmark)
            if not header_dumped:
                header_dumped = True
                sbs.dump_header(stream_csv)
            sbs.dump(stream_csv)
        stream_csv.close()

    """
    Build the map from single benchmark to StaticBenchmarkAnalyzer.
    """

    def analyze_static_stream(self):
        self.static_stream_analyzer = dict()
        for benchmark in self.driver.benchmarks:
            if not isinstance(benchmark, MultiProgramBenchmark):
                print('Start static stream analysis for {b}'.format(
                    b=benchmark.get_name()))
                ssa = StaticStreamAnalyzer(
                    benchmark, self.stream_transform_config)
                self.static_stream_analyzer[benchmark] = ssa

    def analyze_stream_type(self):
        total_average_stream_length = 0.0
        for benchmark in self.driver.benchmarks:
            if benchmark not in self.static_stream_analyzer:
                continue
            ssa = self.static_stream_analyzer[benchmark]
            ssa.analyze_stream_type()

    def analyze_chosen_stream_length(self):
        total_average_stream_length = 0.0
        for benchmark in self.driver.benchmarks:
            if benchmark not in self.static_stream_analyzer:
                continue
            ssa = self.static_stream_analyzer[benchmark]
            average_stream_length = ssa.analyze_chosen_stream_length()
            print('{b} has chosen stream length {l}'.format(
                b=benchmark.get_name(), l=average_stream_length))
            total_average_stream_length += average_stream_length
        print('Average chosen stream length is {l}'.format(
            l=total_average_stream_length/len(self.static_stream_analyzer)))

    def analyze_mismatch_between_chosen_and_static_analysis(self):
        mismatches = [0] * 3
        n_single_benchmarks = 0
        for benchmark in self.driver.benchmarks:
            if benchmark not in self.static_stream_analyzer:
                continue
            n_single_benchmarks += 1
            ssa = self.static_stream_analyzer[benchmark]
            for i in range(len(ssa.mismatches)):
                mismatches[i] += ssa.mismatches[i]
        for i in range(len(mismatches)):
            mismatches[i] /= n_single_benchmarks
            print('Average mismatch rate at level {level} is {mismatch}'.format(
                level=i,
                mismatch=mismatches[i]))

    def analyze_alias_stream_access(self):
        # total_aliased_accesses = 0.0
        # for benchmark in self.driver.benchmarks:
        #     if benchmark not in self.static_stream_analyzer:
        #         continue
        #     ssa = self.static_stream_analyzer[benchmark]
        #     alias_stream_access = ssa.analyze_alias_stream_access()
        #     print('{b} has {v} loop accesses aliased.'.format(
        #         b=benchmark.get_name(), v=alias_stream_access
        #     ))
        #     total_aliased_accesses += alias_stream_access
        # print('Average alias stream accesses is {v}'.format(
        #     v=total_aliased_accesses / len(self.static_stream_analyzer)))
        pass

    def get_single_program_time(self, benchmark, transform_config, simulation_config):
        time = 0.0
        for trace in benchmark.get_traces():
            result = self.driver.get_simulation_result(
                benchmark, trace, transform_config, simulation_config)
            time += result.stats.get_sim_seconds() * trace.weight
        return time

    def analyze_single_benchmark_speedup_matrix(self, benchmark):
        all_gem_forge_system_configs = self.driver.simulation_manager.get_all_gem_forge_system_configs()
        speedup_matrix = dict()
        for gem_forge_system_config in all_gem_forge_system_configs:
            speedup_matrix[gem_forge_system_config] = dict()
        for i in range(len(all_gem_forge_system_configs)):
            gem_forge_system_config_i = all_gem_forge_system_configs[i]
            simulation_time_i = self.get_single_program_time(
                benchmark, gem_forge_system_config_i.transform_config, gem_forge_system_config_i.simulation_config)
            for j in range(i, len(all_gem_forge_system_configs)):
                gem_forge_system_config_j = all_gem_forge_system_configs[j]
                simulation_time_j = self.get_single_program_time(
                    benchmark, gem_forge_system_config_j.transform_config, gem_forge_system_config_j.simulation_config)
                speedup_matrix[gem_forge_system_config_i][gem_forge_system_config_j] = simulation_time_i / simulation_time_j
                speedup_matrix[gem_forge_system_config_j][gem_forge_system_config_i] = simulation_time_j / simulation_time_i
        return speedup_matrix

    def print_speedup_over_base_config(self, speedup_matrix, base_gem_forge_system_config):
        all_gem_forge_system_configs = self.driver.simulation_manager.get_all_gem_forge_system_configs()
        table = SimpleTable.SimpleTable(
            'Benchmark', [c.get_id() for c in all_gem_forge_system_configs], compress_columns=True)
        for benchmark in speedup_matrix:
            speedup = [speedup_matrix[benchmark][base_gem_forge_system_config][c]
                       for c in all_gem_forge_system_configs]
            table.add_row(benchmark.get_name(), speedup)
        table.add_geomean_row()
        print(table)

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


class StaticStreamAnalyzer(object):

    class RegionAnalyzer(object):
        def __init__(self, trace, fn):
            self.trace = trace
            self.streams = dict()
            stream_region = StreamMessage_pb2.StreamRegion()
            f = open(fn)
            stream_region.ParseFromString(f.read())
            f.close()
            for s in stream_region.streams:
                stream_name = s.name.split()[3]
                if stream_name not in self.streams:
                    self.streams[stream_name] = [s]
                else:
                    inserted = False
                    for i in range(len(self.streams[stream_name])):
                        if self.streams[stream_name][i].config_loop_level > s.config_loop_level:
                            continue
                        self.streams[stream_name].insert(i, s)
                        inserted = True
                        break
                    if not inserted:
                        self.streams[stream_name].append(s)

        def get_mismatch_streams_at_level(self, level):
            mismatch_streams = list()
            for stream_name in self.streams:
                streams = self.streams[stream_name]
                if len(streams) <= level:
                    continue
                s = streams[level]
                if s.dynamic_info.is_qualified != s.static_info.is_qualified:
                    mismatch_streams.append(s)
            return mismatch_streams

        def get_total_qualified_mem_accesses(self):
            total = 0
            for stream_name in self.streams:
                s = self.streams[stream_name][0]
                if s.type == 'phi':
                    continue
                if not s.dynamic_info.is_qualified:
                    continue
                total += s.dynamic_info.total_accesses
            return total

        def get_total_aliased_loop_mem_accesses(self):
            total = 0
            for stream_name in self.streams:
                s = self.streams[stream_name][0]
                if s.type == 'phi':
                    continue
                if not s.dynamic_info.is_aliased:
                    continue
                total += s.dynamic_info.total_accesses
            return total

        def get_total_loop_mem_accesses(self):
            total = 0
            for stream_name in self.streams:
                s = self.streams[stream_name][0]
                if s.type == 'phi':
                    continue
                total += s.dynamic_info.total_accesses
            return total

        def get_chosen_streams(self):
            chosen_streams = list()
            for stream_name in self.streams:
                for s in self.streams[stream_name]:
                    if s.dynamic_info.is_chosen:
                        chosen_streams.append(s)
                        break
            return chosen_streams

        def get_inner_most_streams(self):
            streams = list()
            for stream_name in self.streams:
                streams.append(self.streams[stream_name][0])
            return streams

    class TraceAnalyzer(object):
        def __init__(self, parent, trace):
            self.parent = parent
            self.trace = trace
            self.regions = list()
            self.analyze()
            self.total_qualified_accesses = float(sum(
                [r.get_total_qualified_mem_accesses() for r in self.regions]))
            self.total_loop_accesses = float(sum(
                [r.get_total_loop_mem_accesses() for r in self.regions]))
            self.total_alise_accesses = float(sum(
                [r.get_total_aliased_loop_mem_accesses() for r in self.regions]))

        def analyze(self):
            tdg_extra_path = self.parent.benchmark.get_tdg_extra_path(
                self.parent.stream_transform_config, self.trace)
            for item in os.listdir(tdg_extra_path):
                if not os.path.isdir(os.path.join(tdg_extra_path, item)):
                    continue
                stream_region_path = os.path.join(
                    tdg_extra_path, item, 'streams.info')
                self.regions.append(StaticStreamAnalyzer.RegionAnalyzer(
                    self.trace, stream_region_path))
            tdg_stats_fn = self.parent.benchmark.get_tdg(
                self.parent.stream_transform_config, self.trace) + '.stats.txt'
            with open(tdg_stats_fn) as f:
                count = 0
                for line in f:
                    count += 1
                    if count == 3:
                        fields = line.split()
                        self.total_mem_accesses = float(fields[1])
                        break

        def analyze_mismatch_at_level(self, level):
            total_mismatch_mem_accesses = 0
            for r in self.regions:
                mismatch_streams = r.get_mismatch_streams_at_level(level)
                for s in mismatch_streams:
                    dynamic_info = s.dynamic_info
                    static_info = s.static_info
                    if dynamic_info.is_aliased:
                        # Ignore the mismatch due to aliasing.
                        continue
                    if dynamic_info.total_accesses == 0:
                        # Ignore the mismatch due to no accesses in the trace.
                        continue
                    if not dynamic_info.is_qualified:
                        # Ingore the case where static reports stream but dynamic doesn't
                        continue
                    self.print_mismatch(s)
                    if s.type != 'phi':
                        total_mismatch_mem_accesses += dynamic_info.total_accesses
            if self.total_qualified_accesses != 0:
                total_mismatch_mem_accesses /= self.total_qualified_accesses
            return total_mismatch_mem_accesses

        def analyze_chosen_stream_length(self):
            total_stream_accesses = 0.0
            total_stream_length = 0.0
            for r in self.regions:
                chosen_streams = r.get_chosen_streams()
                for s in chosen_streams:
                    if s.type == 'phi':
                        # Ignore IndVarStreams.
                        continue
                    dynamic_info = s.dynamic_info
                    average_length = dynamic_info.total_accesses / dynamic_info.total_configures
                    total_stream_accesses += dynamic_info.total_accesses
                    total_stream_length += dynamic_info.total_accesses * average_length
            if total_stream_accesses != 0:
                return total_stream_length / total_stream_accesses
            else:
                return total_stream_length

        def analyze_stream_type(self):
            stream_types = dict()
            for r in self.regions:
                for s in r.get_inner_most_streams():
                    if s.type == 'phi':
                        # Ignore IndVarStreams.
                        continue
                    dynamic_info = s.dynamic_info
                    static_info = s.static_info
                    val_pattern = static_info.val_pattern
                    if val_pattern not in stream_types:
                        stream_types[val_pattern] = 0.0
                    stream_types[val_pattern] += dynamic_info.total_accesses
            return stream_types

        def print_mismatch(self, s):
            print('Mismatch stream! weight {weight:.2f} lv {lv:1} dynamic {dynamic:1} static {static:1} reason {reason:20}: {s}'.format(
                weight=(s.dynamic_info.total_accesses /
                        self.total_qualified_accesses *
                        self.trace.get_weight()) if self.total_qualified_accesses != 0 else 0.0,
                lv=s.loop_level - s.config_loop_level,
                dynamic=s.dynamic_info.is_qualified,
                static=s.static_info.is_qualified,
                reason=StreamMessage_pb2.StaticStreamInfo.StaticNotStreamReason.Name(
                    s.static_info.not_stream_reason),
                s=s.name
            ))

    def __init__(self, benchmark, stream_transform_config):
        self.benchmark = benchmark
        self.stream_transform_config = stream_transform_config
        self.analyze()

    def analyze(self):
        self.traces = list()
        self.mismatches = list()
        for trace in self.benchmark.get_traces():
            self.traces.append(StaticStreamAnalyzer.TraceAnalyzer(self, trace))
        for level in xrange(3):
            total_mismatch_mem_accesses = 0.0
            for trace in self.traces:
                trace_mismatch_mem_accesses = trace.analyze_mismatch_at_level(
                    level)
                total_mismatch_mem_accesses += trace_mismatch_mem_accesses * trace.trace.get_weight()
            print('========= Analyzing Loop Level {level} Mismatch MemAccesses {weight:.3f}'.format(
                level=level, weight=total_mismatch_mem_accesses))
            self.mismatches.append(total_mismatch_mem_accesses)

    def analyze_chosen_stream_length(self):
        average_stream_length = 0.0
        for trace in self.traces:
            average_stream_length += trace.analyze_chosen_stream_length() * \
                trace.trace.get_weight()
        return average_stream_length

    def analyze_alias_stream_access(self):
        total_aliased_accesses = 0.0
        total_loop_accesses = 0.0
        for trace in self.traces:
            total_aliased_accesses += trace.total_alise_accesses * trace.trace.get_weight()
            total_loop_accesses += trace.total_loop_accesses * trace.trace.get_weight()
        return total_aliased_accesses / total_loop_accesses

    def analyze_stream_type(self):
        total_types = dict()
        total_mem_accesses = 0.0
        for trace in self.traces:
            stream_types = trace.analyze_stream_type()
            for stream_type in stream_types:
                if stream_type not in total_types:
                    total_types[stream_type] = 0.0
                total_types[stream_type] += stream_types[stream_type] * \
                    trace.trace.get_weight()
            total_mem_accesses += trace.total_mem_accesses * trace.trace.get_weight()
        for stream_type in total_types:
            total_types[stream_type] /= total_mem_accesses
        print total_types
        print total_mem_accesses


def analyze(driver):
    se = StreamExperiments(driver)
    se.dump()
