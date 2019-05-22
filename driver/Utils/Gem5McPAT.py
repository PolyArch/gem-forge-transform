
import Gem5Stats

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
        assert(self.hasL1_5())
        # There is L1_5, use Gem5 L2 as McPAT L3.
        self.configureL3Cache(self.gem5Sys['l2'])
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
        for cpu in self.gem5Sys['cpu']:
            self.configureGemForgeCPU(cpu)

        # ! Dual channel have two memory controller,
        # ! But mcpat supports only one.
        self.configureMemoryControl(self.gem5Sys['mem_ctrls'][0])

    def hasL1_5(self):
        for cpu in self.gem5Sys['cpu']:
            if "l1_5dcache" in cpu:
                return True
        return False

    def hasMultipleCPU(self):
        return len(self.gem5Sys['cpu']) > 1

    def getCacheTagAccessLatency(self, cache):
        if cache['sequential_access']:
            return cache['data_latency'] + cache['tag_latency']
        else:
            return max(cache['data_latency'], cache['tag_latency'])

    def getCPUClockDomain(self):
        return self.gem5Sys['cpu_clk_domain']['clock'][0]

    def getSystemClockDomain(self):
        return self.gem5Sys['clk_domain']['clock'][0]

    def toMHz(self, clock):
        return int(1e6 / clock)

    def getTLBSize(self, tlb):
        return tlb['size']

    def configureMemoryControl(self, mc):
        mcpat_mc = self.xml.sys.mc
        mcpat_mc.mc_clock = self.toMHz(self.getSystemClockDomain())
        mcpat_mc.memory_channels_per_mc = mc['channels']
        mcpat_mc.number_ranks = mc['ranks_per_channel']
        mcpat_mc.llc_line_length = mc['write_buffer_size']

    def configureL2Directories(self, cache):
        L2DIdx = self.xml.sys.number_of_L2Directories
        assert(L2DIdx == 0)
        self.xml.sys.number_of_L2Directories += 1
        L2D = self.xml.sys.L2Directory[L2DIdx]
        L2D.clockrate = self.toMHz(self.getCPUClockDomain())

    def configureL2Cache(self, cache):
        tags = cache['tags']
        L2Idx = self.xml.sys.number_of_L2s
        assert(L2Idx == 0)
        self.xml.sys.number_of_L2s += 1
        L2 = self.xml.sys.L2[L2Idx]
        L2.L2_config[0] = cache['size']
        L2.L2_config[1] = tags['block_size']
        L2.L2_config[2] = cache['assoc']
        L2.L2_config[3] = 1
        accessLatency = self.getCacheTagAccessLatency(cache)
        L2.L2_config[4] = accessLatency
        L2.L2_config[5] = accessLatency
        L2.L2_config[6] = 32
        L2.L2_config[7] = 1

        L2.buffer_sizes[0] = cache['mshrs']
        L2.buffer_sizes[1] = cache['mshrs']
        L2.buffer_sizes[2] = cache['mshrs']
        L2.buffer_sizes[3] = cache['write_buffers']
        L2.clockrate = self.toMHz(self.getCPUClockDomain())

    def configureL3Cache(self, cache):
        tags = cache['tags']
        L3Idx = self.xml.sys.number_of_L3s
        assert(L3Idx == 0)
        self.xml.sys.number_of_L3s += 1
        L3 = self.xml.sys.L3[L3Idx]
        L3.L3_config[0] = cache['size']
        L3.L3_config[1] = tags['block_size']
        L3.L3_config[2] = cache['assoc']
        L3.L3_config[3] = 1

        accessLatency = self.getCacheTagAccessLatency(cache)
        L3.L3_config[4] = accessLatency
        L3.L3_config[5] = accessLatency
        L3.L3_config[6] = 32
        L3.L3_config[7] = 1

        L3.buffer_sizes[0] = cache['mshrs']
        L3.buffer_sizes[1] = cache['mshrs']
        L3.buffer_sizes[2] = cache['mshrs']
        L3.buffer_sizes[3] = cache['write_buffers']

        L3.clockrate = self.toMHz(self.getCPUClockDomain())

    def configureGemForgeCPU(self, cpu):
        idx = cpu['cpu_id']
        core = self.xml.sys.core[idx]
        core.clock_rate = self.toMHz(self.getCPUClockDomain())
        core.x86 = True
        core.fetch_width = cpu['fetchWidth']
        core.decode_width = cpu['decodeWidth']
        core.issue_width = cpu['issueWidth']
        core.peak_issue_width = cpu['issueWidth']
        core.commit_width = cpu['commitWidth']
        core.fp_issue_width = min(cpu['issueWidth'], 6)
        # ! Assume all x86
        intExe = 2
        fpExe = 8
        baseStages = cpu['fetchToDecodeDelay'] + cpu['decodeToRenameDelay'] + \
            cpu['renameToIEWDelay'] + cpu['iewToCommitDelay']
        maxBaseStages = max({1, 1, 1, 1})

        core.pipeline_depth[0] = intExe + baseStages + maxBaseStages
        core.pipeline_depth[1] = fpExe + baseStages + maxBaseStages
        core.instruction_buffer_size = cpu['maxFetchQueueSize']
        core.instruction_window_size = cpu['maxInstructionQueueSize']
        core.fp_instruction_window_size = cpu['maxInstructionQueueSize']
        core.ROB_size = cpu['maxReorderBufferSize']
        core.phy_Regs_IRF_size = 256
        core.phy_Regs_FRF_size = 256
        core.store_buffer_size = cpu['storeQueueSize']
        core.load_buffer_size = cpu['loadQueueSize']
        core.opt_local = 0
        core.instruction_length = 32
        core.opcode_width = 16
        core.micro_opcode_width = 8
        core.machine_type = 0  # 0 for O3.

        core.itlb.number_entries = self.getTLBSize(cpu['itb'])
        core.dtlb.number_entries = self.getTLBSize(cpu['dtb'])

        # L1 inst cache.
        instL1 = cpu['icache']
        instL1Tag = instL1['tags']
        mcpatInstL1 = core.icache
        accessLatency = self.getCacheTagAccessLatency(instL1)
        mcpatInstL1.icache_config[0] = instL1['size']          # capacity
        # block_width
        mcpatInstL1.icache_config[1] = instL1Tag['block_size']
        mcpatInstL1.icache_config[2] = instL1['assoc']
        mcpatInstL1.icache_config[3] = 1                           # bank
        mcpatInstL1.icache_config[4] = accessLatency               # throughput
        mcpatInstL1.icache_config[5] = accessLatency               # latency
        # output_width
        mcpatInstL1.icache_config[6] = 32
        # cache_policy
        mcpatInstL1.icache_config[7] = 0
        mcpatInstL1.buffer_sizes[0] = instL1['mshrs']  # MSHR
        mcpatInstL1.buffer_sizes[1] = instL1['mshrs']  # fill_buffer_size
        mcpatInstL1.buffer_sizes[2] = instL1['mshrs']  # prefetch_buffer_size
        mcpatInstL1.buffer_sizes[3] = 0                   # wb_buffer_size

        # L1 data cache.
        dataL1 = cpu['dcache']
        dataL1Tag = dataL1['tags']
        mcpatInstL1 = core.icache
        accessLatency = self.getCacheTagAccessLatency(dataL1)
        mcpatInstL1.icache_config[0] = dataL1['size']          # capacity
        # block_width
        mcpatInstL1.icache_config[1] = dataL1Tag['block_size']
        mcpatInstL1.icache_config[2] = dataL1['assoc']
        mcpatInstL1.icache_config[3] = 1                           # bank
        mcpatInstL1.icache_config[4] = accessLatency               # throughput
        mcpatInstL1.icache_config[5] = accessLatency               # latency
        # output_width
        mcpatInstL1.icache_config[6] = 32
        # cache_policy
        mcpatInstL1.icache_config[7] = 0
        mcpatInstL1.buffer_sizes[0] = dataL1['mshrs']  # MSHR
        mcpatInstL1.buffer_sizes[1] = dataL1['mshrs']  # fill_buffer_size
        mcpatInstL1.buffer_sizes[2] = dataL1['mshrs']  # prefetch_buffer_size
        mcpatInstL1.buffer_sizes[3] = 0                   # wb_buffer_size

        if self.hasL1_5():
            self.configureL2Cache(cpu['l1_5dcache'])
            self.configureL2Directories(cpu['l1_5dcache'])

        # L1 directory.
        L1Directory = self.xml.sys.L1Directory[idx]
        L1Directory.clockrate = self.toMHz(self.getCPUClockDomain())

    def getScalarStats(self, stat):
        return self.stats.get_default(stat, 0)

    def getVecStatsTotal(self, stat):
        return self.stats.get_default(
            stat + '::total',
            0
        )

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
        memReads = self.getVecStatsTotal("system.mem_ctrls.num_reads")
        memWrites = self.getVecStatsTotal("system.mem_ctrls.num_writes")
        mc = self.xml.sys.mc
        mc.memory_reads = memReads
        mc.memory_writes = memWrites
        mc.memory_accesses = memReads + memWrites

    def setStatsL2Cache(self):
        total = self.getVecStatsTotal("system.l2.overall_accesses")
        totalMisses = self.getVecStatsTotal("system.l2.overall_misses")
        writes = self.getVecStatsTotal("system.l2.WritebackClean_accesses") + \
            self.getVecStatsTotal("system.l2.WritebackDirty_accesses")
        writeMisses = self.getVecStatsTotal("system.l2.WritebackClean_misses") + \
            self.getVecStatsTotal("system.l2.WritebackDirty_misses")

        if not self.hasL1_5():
            L2 = self.xml.sys.L2[0]
            L2.read_accesses = total - writes
            L2.write_accesses = writes
            L2.read_misses = totalMisses - writeMisses
            L2.write_misses = writeMisses
        else:
            L3 = self.xml.sys.L3[0]
            L3.read_accesses = total - writes
            L3.write_accesses = writes
            L3.read_misses = totalMisses - writeMisses
            L3.write_misses = writeMisses

    def setStatsGemForgeCPU(self, cpu):
        idx = cpu['cpu_id']

        # If there are multiple cpus, we have to match back the cpu_id
        if self.hasMultipleCPU():
            def scalar(stat): return self.stats.get_default(
                'system.cpu{idx}.{stat}'.format(idx=idx, stat=stat), 0)

            def vector(stat): return self.getVecStatsTotal(
                'system.cpu{idx}.{stat}'.format(idx=idx, stat=stat))
        else:
            def scalar(stat): return self.stats.get_default(
                'system.cpu.{stat}'.format(stat=stat), 0)

            def vector(stat): return self.getVecStatsTotal(
                'system.cpu.{stat}'.format(stat=stat))

        core = self.xml.sys.core[idx]
        totalCycles = scalar("numCycles")
        idleCycles = 0
        branchInsts = scalar("fetch.Branches") * self.int_inst_multiplier
        decodedInsts = scalar("decode.DecodedInsts")
        renameWrites = scalar("rename.RenamedOperands") * self.int_inst_multiplier
        renameReads = scalar("rename.RenameLookups") * self.int_inst_multiplier
        renameFpReads = scalar("rename.fp_rename_lookups") * self.int_inst_multiplier
        renameFpWrites = 0
        robReads = scalar("iew.robReads") * self.int_inst_multiplier
        robWrites = scalar("iew.robWrites") * self.int_inst_multiplier
        instWinIntReads = scalar("iew.intInstQueueReads") * self.int_inst_multiplier
        instWinIntWrites = scalar("iew.intInstQueueWrites") * self.int_inst_multiplier
        instWinIntWakeups = scalar("iew.intInstQueueWakeups") * self.int_inst_multiplier
        instWinFpReads = scalar("iew.fpInstQueueReads")
        instWinFpWrites = scalar("iew.fpInstQueueWrites")
        instWinFpWakeups = scalar("iew.fpInstQueueWakeups")
        intRegReads = scalar("iew.intRegReads") * self.int_inst_multiplier
        intRegWrites = scalar("iew.intRegWrites") * self.int_inst_multiplier
        fpRegReads = scalar("iew.fpRegReads")
        fpRegWrites = scalar("iew.fpRegWrites")
        ALUAccesses = scalar("iew.ALUAccessesCycles")
        MultAccesses = scalar("iew.MultAccessesCycles")
        FPUAccesses = scalar("iew.FPUAccessesCycles")
        loadInsts = scalar("iew.execLoadInsts") * self.int_inst_multiplier
        storeInsts = scalar("iew.execStoreInsts")
        commitInsts = vector("commit.committedInsts")
        commitIntInsts = vector("commit.committedIntInsts") * self.int_inst_multiplier
        commitFpInsts = vector("commit.committedFpInsts")
        commitInsts = commitIntInsts + commitFpInsts
        commitCalls = vector("commit.committedCallInsts")

        core.total_instructions = decodedInsts
        core.int_instructions = 0
        core.fp_instructions = 0
        core.branch_instructions = branchInsts
        core.branch_mispredictions = 0
        core.committed_instructions = commitInsts
        core.committed_int_instructions = commitIntInsts
        core.committed_fp_instructions = commitFpInsts
        core.load_instructions = loadInsts
        core.store_instructions = storeInsts
        core.total_cycles = totalCycles
        core.idle_cycles = idleCycles
        core.busy_cycles = totalCycles - idleCycles
        core.instruction_buffer_reads = 0
        core.instruction_buffer_write = 0
        core.ROB_reads = robReads
        core.ROB_writes = robWrites
        core.rename_reads = renameReads
        core.rename_writes = renameWrites
        core.fp_rename_reads = renameFpReads
        core.fp_rename_writes = renameFpWrites
        core.inst_window_reads = instWinIntReads
        core.inst_window_writes = instWinIntWrites
        core.inst_window_wakeup_accesses = instWinIntWakeups
        core.inst_window_selections = 0
        core.fp_inst_window_reads = instWinFpReads
        core.fp_inst_window_writes = instWinFpWrites
        core.fp_inst_window_wakeup_accesses = instWinFpWakeups
        core.fp_inst_window_selections = 0
        core.archi_int_regfile_reads = 0
        core.archi_float_regfile_reads = 0
        core.phy_int_regfile_reads = 0
        core.phy_float_regfile_reads = 0
        core.phy_int_regfile_writes = 0
        core.phy_float_regfile_writes = 0
        core.archi_int_regfile_writes = 0
        core.archi_float_regfile_writes = 0
        core.int_regfile_reads = intRegReads
        core.float_regfile_reads = fpRegReads
        core.int_regfile_writes = intRegWrites
        core.float_regfile_writes = fpRegWrites
        core.windowed_reg_accesses = 0
        core.windowed_reg_transports = 0
        core.function_calls = commitCalls
        core.context_switches = 0
        core.ialu_accesses = ALUAccesses
        core.mul_accesses = MultAccesses
        core.fpu_accesses = FPUAccesses
        core.cdb_alu_accesses = ALUAccesses
        core.cdb_mul_accesses = MultAccesses
        core.cdb_fpu_accesses = FPUAccesses
        core.load_buffer_reads = 0
        core.load_buffer_writes = 0
        core.load_buffer_cams = 0
        core.store_buffer_reads = 0
        core.store_buffer_writes = 0
        core.store_buffer_cams = 0
        core.store_buffer_forwards = 0
        core.main_memory_access = 0
        core.main_memory_read = 0
        core.main_memory_write = 0
        core.pipeline_duty_cycle = 0

        reads = scalar("dtb.rdAccesses")
        readMisses = scalar("dtb.rdMisses")
        writes = scalar("dtb.wrAccesses")
        writeMisses = scalar("dtb.wrMisses")
        core.dtlb.total_accesses = reads + writes
        core.dtlb.read_accesses = reads
        core.dtlb.write_accesses = writes
        core.dtlb.read_misses = readMisses
        core.dtlb.write_misses = writeMisses
        core.dtlb.total_misses = readMisses + writeMisses
        reads = scalar("itb.rdAccesses")
        readMisses = scalar("itb.rdMisses")
        writes = scalar("itb.wrAccesses")
        writeMisses = scalar("itb.wrMisses")
        core.itlb.total_accesses = reads + writes
        core.itlb.total_misses = readMisses + writeMisses
        writes = vector("dcache.WriteReq_accesses")
        writeMisses = vector("dcache.WriteReq_misses")
        reads = vector("dcache.ReadReq_accesses")
        readMisses = vector("dcache.ReadReq_misses")
        mcpatDataL1 = core.dcache
        mcpatDataL1.read_accesses = reads
        mcpatDataL1.write_accesses = writes
        mcpatDataL1.read_misses = readMisses
        mcpatDataL1.write_misses = writeMisses

        if self.hasL1_5():
            writes = vector("l1_5dcache.WritebackDirty_accesses") + \
                vector("l1_5dcache.WritebackClean_accesses")
            writeHits = vector("l1_5dcache.WritebackDirty_hits") + \
                vector("l1_5dcache.WritebackClean_hits")
            writeMisses = writes - writeHits
            reads = vector("l1_5dcache.ReadReq_accesses") + \
                vector("l1_5dcache.ReadExReq_accesses") + \
                vector("l1_5dcache.ReadSharedReq_accesses")
            readMisses = vector("l1_5dcache.ReadReq_misses") + \
                vector("l1_5dcache.ReadExReq_misses") + \
                vector("l1_5dcache.ReadSharedReq_misses")
            L2 = self.xml.sys.L2[idx]
            L2.read_accesses = reads
            L2.write_accesses = writes
            L2.read_misses = readMisses
            L2.write_misses = writeMisses

        reads = vector("icache.ReadReq_accesses")
        readMisses = vector("icache.ReadReq_misses")
        mcpatInstL1 = core.icache
        mcpatInstL1.read_accesses = reads
        mcpatInstL1.read_misses = readMisses
        # * For LLVMTraceCPU, there is no instruction cache simulated,
        # * we added the number of instructions as an approximation.
        fetchedInsts = scalar("fetch.Insts")
        mcpatInstL1.read_accesses = fetchedInsts / 9
        mcpatInstL1.read_misses = fetchedInsts / 1000
