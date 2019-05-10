import Utils.Gem5Stats as Gem5Stats
import Utils.SimpleTable

import os


class Record(object):
    def __init__(self):
        self.cycles = 0.0
        self.ops = 0.0
        self.opc = 0.0
        self.insts = 0.0
        self.ipc = 0.0
        self.loads = 0.0
        self.stores = 0.0
        self.dcache_reads = 0.0
        self.dcache_writes = 0.0
        self.dcache_miss_lat = 0.0
        self.llc_demmands = 0.0
        self.llc_misses = 0.0
        self.branches = 0.0
        self.miss_branches = 0.0
        self.alus = 0.0
        self.int_mults = 0.0
        self.int_divs = 0.0
        self.float_adds = 0.0
        self.float_mults = 0.0
        self.float_cvts = 0.0

    def addCommonResult(self, result):
        self.cycles += result.stats.get_default('system.cpu.numCycles', 0)
        self.dcache_reads += result.stats.get_default(
            'system.cpu.dcache.ReadReq_accesses::total', 0)
        self.dcache_writes += result.stats.get_default(
            'system.cpu.dcache.WriteReq_accesses::total', 0)
        self.dcache_miss_lat += result.stats.get_default(
            'system.cpu.dcache.overall_miss_latency::total', 0) 
        self.llc_demmands += result.stats.get_default(
            'system.l2.demand_accesses::total', 0)
        self.llc_misses += result.stats.get_default(
            'system.l2.demand_misses::total', 0)

    def updateDerivedFields(self):
        if self.cycles != 0:
            self.opc = self.ops / self.cycles
            self.ipc = self.insts / self.cycles

    def addValidResult(self, result):
        self.addCommonResult(result)
        self.ops += result.stats.get_default('system.cpu.committedOps', 0)
        self.insts += result.stats.get_default('system.cpu.committedInsts', 0)
        self.loads += result.stats.get_default(
            'system.cpu.commit.loads', 0)
        self.stores += result.stats.get_default(
            'system.cpu.iew.exec_stores', 0)
        self.branches += result.stats.get_default(
            'system.cpu.commit.branches', 0)
        self.miss_branches += result.stats.get_default(
            'system.cpu.iew.branchMispredicts', 0)
        self.alus += result.stats.get_default(
            'system.cpu.iq.FU_type_0::IntAlu', 0)
        self.int_mults += result.stats.get_default(
            'system.cpu.iq.FU_type_0::IntMult', 0)
        self.int_divs += result.stats.get_default(
            'system.cpu.iq.FU_type_0::IntDiv', 0)
        self.float_adds += result.stats.get_default(
            'system.cpu.iq.FU_type_0::FloatAdd', 0)
        self.float_mults += result.stats.get_default(
            'system.cpu.iq.FU_type_0::FloatMult', 0)
        self.float_cvts += result.stats.get_default(
            'system.cpu.iq.FU_type_0::FloatCvt', 0)

    def addReplayResult(self, result):
        self.addCommonResult(result)
        self.ops += result.stats.get_default(
            'system.cpu.commit.committedOps', 0)
        self.insts += result.stats.get_default(
            'system.cpu.commit.committedOps', 0)
        self.loads += result.stats.get_default(
            'system.cpu.dcache.ReadReq_accesses::total', 0)
        self.stores += result.stats.get_default(
            'system.cpu.dcache.WriteReq_accesses::total', 0)
        self.branches += result.stats.get_default(
            'system.cpu.fetch.Branches', 0)
        self.miss_branches += result.stats.get_default(
            'system.cpu.fetch.branchPredMisses', 0)
        self.alus += result.stats.get_default(
            'system.cpu.iew.FU_type_0::IntAlu', 0)
        self.int_mults += result.stats.get_default(
            'system.cpu.iew.FU_type_0::IntMult', 0)
        self.int_divs += result.stats.get_default(
            'system.cpu.iew.FU_type_0::IntDiv', 0)
        self.float_adds += result.stats.get_default(
            'system.cpu.iew.FU_type_0::FloatAdd', 0)
        self.float_mults += result.stats.get_default(
            'system.cpu.iew.FU_type_0::FloatMult', 0)
        self.float_cvts += result.stats.get_default(
            'system.cpu.iew.FU_type_0::FloatCvt', 0)

    def dumpHeader(self, f):
        fields = vars(self).keys()
        fields.sort()
        for x in fields:
            f.write('{x},'.format(x=x))
        f.write('\n')

    def dumpValue(self, f):
        self.updateDerivedFields()
        record = vars(self)
        fields = record.keys()
        fields.sort()
        for x in fields:
            f.write('{v},'.format(v=record[x]))
        f.write('\n')


class ValidExperiments(object):
    def __init__(self, driver):
        self.driver = driver
        self.replay_transform_config = self.driver.transform_manager.get_config(
            'replay')
        self.valid_transform_config = self.driver.transform_manager.get_config(
            'valid')

        replay_simulation_configs = self.driver.simulation_manager.get_configs(
            'replay')
        assert(len(replay_simulation_configs) == 1)
        self.replay_simulation_config = replay_simulation_configs[0]
        valid_simulation_configs = self.driver.simulation_manager.get_configs(
            'valid')
        assert(len(valid_simulation_configs) == 1)
        self.valid_simulation_config = valid_simulation_configs[0]

        self.result_f = open('valid.csv', 'w')
        for benchmark in self.driver.benchmarks:
            self.analyzeSingleBenchmark(benchmark)
        self.result_f.close()

    def getSingleProgramRecord(self, benchmark, transform_config, simulation_config):
        record = Record()
        for trace in benchmark.get_traces():
            result = self.driver.get_simulation_result(
                benchmark, trace, transform_config, simulation_config)
            if transform_config.get_transform_id() == 'valid':
                record.addValidResult(result)
            elif transform_config.get_transform_id() == 'replay':
                record.addReplayResult(result)
            else:
                raise ValueError('Invalid transform result.')
        return record

    def analyzeSingleBenchmark(self, benchmark):
        print('Start to analyze {benchmark}'.format(
            benchmark=benchmark.get_name()))
        replay_record = self.getSingleProgramRecord(
            benchmark,
            self.replay_transform_config,
            self.replay_simulation_config
        )
        valid_record = self.getSingleProgramRecord(
            benchmark,
            self.valid_transform_config,
            self.valid_simulation_config
        )
        self.result_f.write('{b}\n'.format(b=benchmark.get_name()))
        valid_record.dumpHeader(self.result_f)
        valid_record.dumpValue(self.result_f)
        replay_record.dumpValue(self.result_f)


def analyze(driver):
    ValidExperiments(driver)
