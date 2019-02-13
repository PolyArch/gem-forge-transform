import numpy


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

    def dump_csv(self, csv, simulation_configs):
        csv.write(
            '{b},{t},{r},{w},{l1},{l2},{mem}'.format(
                b=self.benchmark_name,
                t=self.trace_id,
                r=self.region_id,
                w=self.weight,
                l1=self.l1,
                l2=self.l2,
                mem=self.mem,
            )
        )
        for simulation_config in simulation_configs:
            ipc = self.sim_ipcs[simulation_config]
            csv.write(',{ipc}'.format(ipc=ipc))
        csv.write('\n')

    @staticmethod
    def dump_csv_header(csv, simulation_configs):
        csv.write('benchmark,trace,region,weight,l1,l2,mem')
        for simulation_config in simulation_configs:
            csv.write(',{s}'.format(s=simulation_config.get_simulation_id()))
        csv.write('\n')


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
        self.simulation_configs = self.driver.simulation_manager.get_configs(
            'adfa')

        self.suite_csv = open(driver.get_unique_id() + '.csv', 'w')
        suite_title = [
            'benchmark',
            'coverage',
            'smart',
        ] + [s.get_simulation_id() for s in self.simulation_configs]
        self.suite_csv.write(','.join(suite_title))
        self.suite_csv.write('\n')

        self.benchmark_trace_csv = open(
            driver.get_unique_id() + '.trace.csv', 'w')
        suite_title = [
            'benchmark',
            'coverage',
        ] + [s.get_simulation_id() for s in self.simulation_configs]
        self.benchmark_trace_csv.write(','.join(suite_title))
        self.benchmark_trace_csv.write('\n')

        self.all_region_csv = open(
            driver.get_unique_id() + '.regions.csv', 'w')
        Region.dump_csv_header(self.all_region_csv, self.simulation_configs)

        for benchmark in self.driver.benchmarks:
            print('Start to analyze {benchmark}'.format(
                benchmark=benchmark.get_name()))
            self.benchmark_csv = open(benchmark.get_name() + '.csv', 'w')
            Region.dump_csv_header(self.benchmark_csv, self.simulation_configs)

            weighted_trace_time = numpy.zeros(len(self.simulation_configs))

            weighted_time = numpy.zeros(len(self.simulation_configs) + 1)
            coverage = 0.0
            for trace in benchmark.get_traces():
                trace_coverage, trace_weighted_time = self.analyzeBenchmarkTrace(
                    benchmark, trace)
                coverage += trace_coverage
                weighted_time += trace_weighted_time
                weighted_trace_time += self.getWeightedTraceTime(
                    benchmark, trace)

            self.suite_csv.write('{b},{w}'.format(
                b=benchmark.get_name(),
                w=coverage,
            ))
            for t in weighted_time:
                self.suite_csv.write(',{t}'.format(t=t))
            self.suite_csv.write('\n')

            self.benchmark_trace_csv.write('{b},{w}'.format(
                b=benchmark.get_name(),
                w=coverage,
            ))
            for t in weighted_trace_time:
                self.benchmark_trace_csv.write(',{t}'.format(t=t))
            self.benchmark_trace_csv.write('\n')

            self.benchmark_csv.close()
        self.suite_csv.close()
        self.benchmark_trace_csv.close()
        self.all_region_csv.close()

    def getWeightedTraceTime(self, benchmark, trace):
        simulation_results = [
            self.driver.get_simulation_result(
                benchmark, trace, self.adfa_transform_config, simulation_config)
            for simulation_config in self.simulation_configs
        ]
        return numpy.array([trace.weight * s.stats.get_sim_seconds() for s in simulation_results])

    def analyzeBenchmarkTrace(self, benchmark, trace):
        simulation_results = [
            self.driver.get_simulation_result(
                benchmark, trace, self.adfa_transform_config, simulation_config)
            for simulation_config in self.simulation_configs
        ]

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
                if simulation_result.simulation_config.get_simulation_id() == 'adfa.ns.unroll':
                    region.l1 = simulation_region_stats.get_region_mem_access() / region.dynamic_insts
                    region.l2 = simulation_region_stats.get_region_l1_misses() / region.dynamic_insts
                    region.mem = simulation_region_stats.get_region_l2_misses() / region.dynamic_insts

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
            region.dump_csv(self.benchmark_csv, self.simulation_configs)
            region.dump_csv(self.all_region_csv, self.simulation_configs)

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


def analyze(driver):
    print('Start analyze ADFA results.')
    ADFAExperiments(driver)
