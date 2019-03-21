import numpy
from Utils.CSVWriter import CSVWriter


class Region(object):
    def __init__(self, **kwargs):
        for key in kwargs:
            setattr(self, key, kwargs[key])

        self.benchmark_name = ''
        self.trace_id = ''
        self.weight = 0.0
        self.sim_ipcs = dict()
        self.l1 = 0.0
        self.l2 = 0.0
        self.mem = 0.0
        self.branch = 0.0
        self.branch_misses = 0.0
        # Threshold choice.
        self.threshold_choice = 'None'
        # Fastest choice.
        self.fastest_choice = 'None'

    def get_row(self, simulation_configs):
        row = [
            self.benchmark_name,
            self.trace_id,
            self.region_id,
            self.weight,
            self.l1,
            self.l2,
            self.mem,
            self.branch,
            self.branch_misses
        ]
        for simulation_config in simulation_configs:
            ipc = self.sim_ipcs[simulation_config]
            row.append(ipc)
        row.append(self.threshold_choice)
        row.append(self.fastest_choice)
        return row

    @staticmethod
    def get_csv_header(simulation_configs):
        title = ['benchmark', 'trace', 'region',
                 'weight', 'l1', 'l2', 'mem', 'br', 'br_miss']
        for simulation_config in simulation_configs:
            title.append('{s}'.format(s=simulation_config.get_simulation_id()))
        title.append('threshold_choice')
        title.append('fastest_choice')
        return title


class ADFAStats(object):
    def __init__(self, tdg_fn):
        self.stats_fn = tdg_fn + '.stats.txt'
        self.regions = list()
        with open(self.stats_fn, 'r') as f:
            lines = 0
            for line in f:
                lines += 1
                if lines <= 2:
                    continue
                fields = line.split()

                configs = int(fields[4])
                dataflows = int(fields[5])
                region_id = fields[0]
                dynamic_insts = int(fields[2])
                dataflow_dynamic_insts = int(fields[3])
                print('Found region {r}'.format(r=region_id))
                self.regions.append(Region(
                    region_id=region_id,
                    dynamic_insts=dynamic_insts,
                    configs=configs,
                    dataflows=dataflows,
                    dataflow_dynamic_insts=dataflow_dynamic_insts
                ))


class ADFAExperiments(object):
    def __init__(self, driver):
        self.driver = driver
        self.adfa_transform_config = self.driver.transform_manager.get_config(
            'adfa')
        self.simulation_configs = list()
        self.simulation_configs += self.driver.simulation_manager.get_configs(
            'replay')
        self.simulation_configs += self.driver.simulation_manager.get_configs(
            'simd')
        self.simulation_configs += self.driver.simulation_manager.get_configs(
            'adfa')

        suite_title = [
            'benchmark',
            'coverage',
            'smart',
        ] + [s.get_simulation_id() for s in self.simulation_configs]
        self.suite_csv = CSVWriter(
            driver.get_unique_id() + '.csv', suite_title)
        self.suite_speedup_csv = CSVWriter(
            driver.get_unique_id() + '.spd.csv', suite_title)

        suite_title = [
            'benchmark',
            'coverage',
        ] + [s.get_simulation_id() for s in self.simulation_configs]
        self.benchmark_trace_csv = CSVWriter(
            driver.get_unique_id() + '.trace.csv', suite_title)

        self.all_region_csv = CSVWriter(
            driver.get_unique_id() + '.regions.csv', Region.get_csv_header(self.simulation_configs))

        suite_title = [
            'benchmark', 'Out', 'None'
        ] + [s.get_simulation_id() for s in self.simulation_configs]
        self.benchmark_threshold_choice_csv = CSVWriter(
            driver.get_unique_id() + '.threshold_choice.csv', suite_title)
        self.benchmark_fastest_choice_csv = CSVWriter(
            driver.get_unique_id() + '.fastest_choice.csv', suite_title)

        for benchmark in self.driver.benchmarks:
            print('Start to analyze {benchmark}'.format(
                benchmark=benchmark.get_name()))

            self.benchmark_csv = CSVWriter(
                benchmark.get_name() + '.csv', Region.get_csv_header(self.simulation_configs))

            weighted_trace_time = numpy.zeros(len(self.simulation_configs))
            weighted_time = numpy.zeros(len(self.simulation_configs) + 1)
            coverage = 0.0

            self.benchmark_threshold_choice = dict()
            self.benchmark_fastest_choice = dict()
            for s in self.simulation_configs:
                self.benchmark_threshold_choice[s.get_simulation_id()] = 0.0
                self.benchmark_fastest_choice[s.get_simulation_id()] = 0.0
            self.benchmark_threshold_choice['None'] = 0.0
            self.benchmark_fastest_choice['None'] = 0.0

            for trace in benchmark.get_traces():
                trace_coverage, trace_weighted_time = self.analyzeBenchmarkTrace(
                    benchmark, trace)
                coverage += trace_coverage
                weighted_time += trace_weighted_time
                weighted_trace_time += self.getWeightedTraceTime(
                    benchmark, trace)

            baseline_idx = -1
            for i in xrange(len(self.simulation_configs)):
                simulation_config = self.simulation_configs[i]
                if simulation_config.get_simulation_id() == 'o8':
                    baseline_idx = i
                    print baseline_idx
                    break

            self.suite_csv.writerow(
                [benchmark.get_name(), coverage] + weighted_time.tolist())
            if baseline_idx != -1:
                self.suite_speedup_csv.writerow(
                    [benchmark.get_name(), coverage] + (weighted_time[baseline_idx + 1] / weighted_time).tolist())

            self.benchmark_trace_csv.writerow(
                [benchmark.get_name(), coverage] + weighted_trace_time.tolist())

            self.benchmark_threshold_choice_csv.writerow(
                [
                    benchmark.get_name(),
                    1.0 - sum(self.benchmark_threshold_choice.itervalues()),
                    self.benchmark_threshold_choice['None']
                ] + [
                    self.benchmark_threshold_choice[s.get_simulation_id()] for s in self.simulation_configs
                ]
            )
            self.benchmark_fastest_choice_csv.writerow(
                [
                    benchmark.get_name(),
                    1.0 - sum(self.benchmark_fastest_choice.itervalues()),
                    self.benchmark_fastest_choice['None']
                ] + [
                    self.benchmark_fastest_choice[s.get_simulation_id()] for s in self.simulation_configs
                ]
            )

            self.benchmark_csv.close()
        self.suite_csv.close()
        self.suite_speedup_csv.close()
        self.benchmark_trace_csv.close()
        self.benchmark_threshold_choice_csv.close()
        self.benchmark_fastest_choice_csv.close()
        self.all_region_csv.close()

    def getSimulationResults(self, benchmark, trace):
        simulation_results = list()
        for simulation_config in self.simulation_configs:
            transform_config = self.adfa_transform_config
            if simulation_config.get_simulation_id() == 'o8':
                transform_config = self.driver.transform_manager.get_config(
                    'replay')
            elif simulation_config.get_simulation_id() == 'simd.o8':
                transform_config = self.driver.transform_manager.get_config(
                    'simd')
            simulation_results.append(self.driver.get_simulation_result(
                benchmark, trace, transform_config, simulation_config))
        return simulation_results

    def getWeightedTraceTime(self, benchmark, trace):
        simulation_results = self.getSimulationResults(benchmark, trace)
        return numpy.array([trace.weight * s.stats.get_sim_seconds() for s in simulation_results])

    def analyzeBenchmarkTrace(self, benchmark, trace):
        simulation_results = self.getSimulationResults(benchmark, trace)

        weighted_time = numpy.zeros(len(simulation_results) + 1)
        coverage = 0.0

        adfa_stats = ADFAStats(benchmark.get_tdg(
            self.adfa_transform_config, trace))

        interested_regions = list()

        for region in adfa_stats.regions:
            print('Processing region {r}'.format(r=region.region_id))
            if region.configs == 0:
                # Ignore those never configured regions.
                continue
            if region.dataflow_dynamic_insts < 10000:
                # We consider this region is too small for accurate results.
                continue

            if not self.checkIfRootRegion(adfa_stats.regions, region, simulation_results[0]):
                continue

            interested_regions.append(region)

            region_id = region.region_id
            region_stats = [
                s.region_stats.regions[region_id] for s in simulation_results
            ]

            # Populate some fields in the region.
            region.weight = self.computeRegionWeight(trace, region)
            region.benchmark_name = benchmark.get_name()
            region.trace_id = trace.get_trace_id()

            # Calculate the IPC for this region.
            for simulation_result in simulation_results:
                simulation_region_stats = simulation_result.region_stats.regions[region_id]
                simulation_cycles = \
                    simulation_region_stats.get_cpu_cycles()
                region.sim_ipcs[simulation_result.simulation_config] = region.dynamic_insts / \
                    simulation_cycles
                # This should be the same across different simulation configs?
                # Well I was wrong: not for idea one.
                if simulation_result.simulation_config.get_simulation_id() == 'o8':
                    region.l1 = simulation_region_stats.get_region_mem_access() / \
                        region.dynamic_insts * 1e3
                    region.l2 = simulation_region_stats.get_region_l1_misses() / \
                        region.dynamic_insts * 1e3
                    region.mem = simulation_region_stats.get_region_l2_misses() / \
                        region.dynamic_insts * 1e3
                    region.branch = simulation_region_stats.get_branches() / region.dynamic_insts * 1e3
                    region.branch_misses = simulation_region_stats.get_branch_misses() / \
                        region.dynamic_insts * 1e3

            region_times = [
                s.get_sim_seconds() for s in region_stats
            ]

            # Pick up the smart time.
            smart_time = None
            for i in xrange(len(region_times)):
                if self.simulation_configs[i].get_simulation_id() == 'adfa.idea':
                    continue
                if smart_time is None:
                    smart_time = region_times[i]
                elif smart_time > region_times[i]:
                    smart_time = region_times[i]
            assert(smart_time is not None)
            region_times.insert(0, smart_time)

            weighted_time += numpy.array([t *
                                          region.weight for t in region_times])
            coverage += region.weight

        for region in interested_regions:
            # Compute the choice for these regions.
            region.threshold_choice, region.fastest_choice = self.computeChoiceForRegion(
                region, 0.5)
            self.benchmark_threshold_choice[region.threshold_choice] += region.weight
            self.benchmark_fastest_choice[region.fastest_choice] += region.weight

        for region in interested_regions:
            self.benchmark_csv.writerow(
                region.get_row(self.simulation_configs))
            self.all_region_csv.writerow(
                region.get_row(self.simulation_configs))

        return (coverage, weighted_time)

    def computeRegionWeight(self, trace, region):
        # So far we know that each trace is roughtly 1e7 instructions.
        return trace.weight * (region.dynamic_insts / 1.0e7)

    def checkIfRootRegion(self, regions, region, simulation_result):
        region_id = region.region_id
        parent_map = simulation_result.region_stats.region_parent
        if region_id in parent_map:
            parent_id = parent_map[region_id]
            # Check if the parent is configured.
            for r in regions:
                if r.region_id != parent_id:
                    continue
                if r.configs != 0:
                    # The parent is also configured, ignore this region.
                    return False
                break
        return True

    """
    Find the best choice of configurations for this region.
    Compared to the ideal ADFA ipc, the first configuration to reach the 
    threshold is chosen as the best one.

    BASE
    SIMD
    NSDF.UNROLL
    NSDF.UNROLL.BANK
    TLS
    SDF.UNROLL
    SDF.UNROLL.BANK
    SDF.UNROLL.BANK.BW
    """

    def computeChoiceForRegion(self, region, threshold):
        temp_ipcs = dict()
        for s, ipc in region.sim_ipcs.iteritems():
            temp_ipcs[s.get_simulation_id()] = ipc
        idea_simulation_id = 'adfa.s.unroll.bank.bw'
        if idea_simulation_id not in temp_ipcs:
            return ('None', max(temp_ipcs.iteritems(), key=lambda x: x[1])[0])
        idea_ipc = temp_ipcs[idea_simulation_id]
        order = [
            'o8',
            'simd.o8',
            'adfa.ns.unroll',
            'adfa.ns.unroll.bank',
            'adfa.tls',
            'adfa.s.unroll',
            'adfa.s.unroll.bank',
        ]
        fastest_choice = 'None'
        threshold_choice = 'None'
        for k in order:
            if k not in temp_ipcs:
                continue
            ipc = temp_ipcs[k]
            if fastest_choice == 'None' or ipc > temp_ipcs[fastest_choice]:
                fastest_choice = k
            if threshold_choice == 'None' and ipc > idea_ipc * threshold:
                threshold_choice = k
        return (threshold_choice, fastest_choice)


def analyze(driver):
    print('Start analyze ADFA results.')
    ADFAExperiments(driver)
