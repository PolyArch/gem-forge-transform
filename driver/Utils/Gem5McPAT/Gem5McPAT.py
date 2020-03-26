
import Utils.Gem5Stats as Gem5Stats

import Utils.Gem5McPAT.ClassicCacheMcPAT as ccache
import Utils.Gem5McPAT.GemForgeCPUMcPAT as gfcpu
import Utils.Gem5McPAT.MemoryControllerMcPAT as memctrl

import mcpat
import os
import json


class Gem5McPAT(object):
    def __init__(self, gem5_out_folder):
        self.folder = gem5_out_folder
        basename = os.path.basename(gem5_out_folder)
        if basename.find('.ipf') != -1:
            self.is_ideal_prefetch = True
            self.int_inst_multiplier = 1.2
        else:
            self.is_ideal_prefetch = False
            self.int_inst_multiplier = 1.0
        
        self.xml = mcpat.ParseXML()
        self.xml.initialize()

        self.xml.sys.core_tech_node = 90
        self.xml.sys.device_type = 0
        self.xml.sys.number_of_L2s = 0
        self.xml.sys.number_of_L3s = 0
        self.xml.sys.number_of_NoCs = 0
        self.xml.sys.number_of_L2Directories = 0

        self.configure()
        self.computeEnergy()

    def configure(self):
        config_fn = os.path.join(self.folder, 'config.json')
        with open(config_fn) as f:
            self.gem5Sys = json.load(f)['system']
        mcpatSys = self.xml.sys
        # Configure the LLC.
        # There is L1_5, use Gem5 L2 as McPAT L3.
        self.configureL3Cache()
        mcpatSys.Private_L2 = True

        mcpatSys.number_of_cores = len(self.gem5Sys['cpu'])
        mcpatSys.number_of_L1Directories = 0
        mcpatSys.number_of_L2Directories = 0
        mcpatSys.number_of_NoCs = 0
        mcpatSys.homogeneous_cores = 1
        mcpatSys.core_tech_node = 65
        mcpatSys.target_core_clockrate = 1000
        mcpatSys.temperature = 380
        mcpatSys.number_cache_levels = 2 if 'l2' in self.gem5Sys else 1
        mcpatSys.interconnect_projection_type = 0
        mcpatSys.device_type = 0
        mcpatSys.longer_channel_device = 1
        mcpatSys.power_gating = 0

        # x86 Only.
        mcpatSys.machine_bits = 32
        mcpatSys.virtual_address_width = 32
        mcpatSys.physical_address_width = 32
        mcpatSys.virtual_memory_page_size = 4096

        # Configure CPUs.
        self.configureCPUs()

        # ! Dual channel have two memory controller,
        # ! But mcpat supports only one.
        self.configureMemoryControl()

    def hasRuby(self):
        return 'ruby' in self.gem5Sys

    def hasFutureCPU(self):
        return 'future_cpus' in self.gem5Sys

    def hasL1_5(self):
        for cpu in self.gem5Sys['cpu']:
            if "l1_5dcache" in cpu:
                return True
        return False

    def hasMultipleCPU(self):
        return len(self.gem5Sys['cpu']) > 1

    def getCPUClockDomain(self):
        return self.gem5Sys['cpu_clk_domain']['clock'][0]

    def getSystemClockDomain(self):
        return self.gem5Sys['clk_domain']['clock'][0]

    def toMHz(self, clock):
        return int(1e6 / clock)

    def getTLBSize(self, tlb):
        return tlb['size']

    def configureMemoryControl(self):
        memctrl.configureMemoryControl(self)

    def configureCPUs(self):
        # Prefer future_cpus as they are detailed cpu.
        if self.hasFutureCPU():
            cpus = self.gem5Sys['future_cpus']
        else:
            cpus = self.gem5Sys['cpu']
        for cpu in cpus:
            self.configureCPU(cpu)

    def configureCPU(self, cpu):
        cpuType = cpu['type']
        if cpuType == 'LLVMTraceCPU':
            gfcpu.configureGemForgeCPU(self, cpu)
        # Configure the L1 caches
        self.configureCPUPrivateCache(cpu)

    def configureCPUPrivateCache(self, cpu):
        cpuId = cpu['cpu_id']
        if self.hasRuby():
            pass
        else:
            # Private is always child of "cpu", not "future_cpu"
            instL1 = self.gem5Sys['cpu'][cpuId]['icache']
            ccache.configureL1ICache(self, cpuId, instL1)
            dataL1 = self.gem5Sys['cpu'][cpuId]['dcache']
            ccache.configureL1DCache(self, cpuId, dataL1)
            if self.hasL1_5():
                L2 = self.gem5Sys['cpu'][cpuId]['l1_5dcache']
                ccache.configureL2Cache(self, L2)
                ccache.configureL2Directories(self, L2)


    def configureL3Cache(self):
        if self.hasRuby():
            pass
        else:
            assert(self.hasL1_5())
            ccache.configureL3Cache(self, self.gem5Sys['l2'])


    def getScalarStats(self, stat):
        if stat not in self.stats:
            print('Warn! Failed to get Stat {stat}'.format(stat=stat))
        return self.stats.get_default(stat, 0)

    def getVecStatsTotal(self, stat):
        total_stat = stat + '::total'
        return self.getScalarStats(total_stat)

    def getCPUStatPrefix(self, cpu_id):
        cpu_name = 'future_cpus' if self.hasFutureCPU() else 'cpu'
        if self.hasMultipleCPU():
            return 'system.{cpu_name}{cpu_id}'.format(
                cpu_name=cpu_name,
                cpu_id=cpu_id,
            )
        else:
            return 'system.{cpu_name}'.format(cpu_name=cpu_name)

    def getCPUPrivateCacheStatPrefix(self, cpu_id):
        cpu_name = 'cpu'
        if self.hasMultipleCPU():
            return 'system.{cpu_name}{cpu_id}'.format(
                cpu_name=cpu_name,
                cpu_id=cpu_id,
            )
        else:
            return 'system.{cpu_name}'.format(cpu_name=cpu_name)

    def getCPUScalarStat(self, stat, cpuId):
        return self.getScalarStats('.'.join([
            self.getCPUStatPrefix(cpuId), stat]))

    def getCPUVectorStat(self, stat, cpuId):
        return self.getVecStatsTotal('.'.join([
            self.getCPUStatPrefix(cpuId), stat]))

    def computeEnergy(self):
        stats_fn = os.path.join(self.folder, 'stats.txt')
        self.stats = Gem5Stats.Gem5Stats(None, stats_fn)
        self.setStatsSystem()
        self.setStatsMemoryControl()
        self.setStatsL2Cache()
        for cpu in self.gem5Sys['cpu']:
            self.setStatsGemForgeCPU(cpu)
        mcpat_processor = mcpat.McPATProcessor(self.xml)
        mcpat_fn = os.path.join(self.folder, 'mcpat.txt')
        mcpat_processor.dumpToFile(mcpat_fn)

    def setStatsSystem(self):
        sys = self.xml.sys
        ticks = self.stats['sim_ticks']
        cycles = ticks / self.getCPUClockDomain()
        sys.total_cycles = cycles

    def setStatsMemoryControl(self):
        memctrl.setStatsMemoryControl(self)

    def setStatsL2Cache(self):
        if self.hasRuby():
            pass
        else:
            ccache.setStatsL2Cache(self)

    def setStatsGemForgeCPU(self, cpu):
        cpuType = cpu['type']
        if cpuType == 'LLVMTraceCPU':
            gfcpu.setStatsGemForgeCPU(self, cpu)