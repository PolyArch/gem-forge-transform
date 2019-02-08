import numpy


class Region(object):
    def __init__(self, **kwargs):
        for key in kwargs:
            setattr(self, key, kwargs[key])


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
        self.suite_csv = open(driver.options.suite + '.csv', 'w')
        suite_title = [
            'benchmark'
        ] + [s.get_simulation_id() for s in self.simulation_configs]
        self.suite_csv.write(','.join(suite_title))
        self.suite_csv.write('\n')
        for benchmark in self.driver.benchmarks:
            print('Start to analyze {benchmark}'.format(
                benchmark=benchmark.get_name()))
            self.benchmark_csv = open(benchmark.get_name() + '.csv', 'w')
            # Write the title
            title = [
                'trace',
                'region',
                'weight',
            ] + [s.get_simulation_id() for s in self.simulation_configs]
            self.benchmark_csv.write(','.join(title))
            self.benchmark_csv.write('\n')

            weighted_time = numpy.zeros(len(self.simulation_configs))
            for trace in benchmark.get_traces():
                weighted_time += self.analyzeBenchmarkTrace(benchmark, trace)

            self.suite_csv.write(benchmark.get_name())
            for t in weighted_time:
                self.suite_csv.write(',{t}'.format(t=t))
            self.suite_csv.write('\n')

            self.benchmark_csv.close()
        self.suite_csv.close()

    def analyzeBenchmarkTrace(self, benchmark, trace):
        simulation_results = [
            self.driver.get_simulation_result(
                benchmark, trace, self.adfa_transform_config, simulation_config)
            for simulation_config in self.simulation_configs
        ]

        weighted_time = numpy.zeros(len(simulation_results))

        adfa_stats = ADFAStats(benchmark.get_tdg(
            self.adfa_transform_config, trace))
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

            region_id = region.region_id
            region_stats = [
                s.region_stats.regions[region_id] for s in simulation_results
            ]
            region_weight = self.computeRegionWeight(trace, region)
            region_times = [
                str(s.get_sim_seconds()) for s in region_stats
            ]
            trace_times = [
                s.stats.get_sim_seconds() for s in simulation_results
            ]
            self.benchmark_csv.write(
                '{t},{r},{w},'.format(
                    t=trace.get_trace_id(),
                    r=region_id,
                    w=region_weight
                )
            )
            self.benchmark_csv.write(','.join(region_times))
            self.benchmark_csv.write('\n')

            weighted_time += numpy.array([t *
                                          trace.weight for t in trace_times])

        return weighted_time

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
