
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
    # L1 directory.
    L1Directory = self.xml.sys.L1Directory[idx]
    L1Directory.clockrate = self.toMHz(self.getCPUClockDomain())

def setStatsGemForgeCPU(self, cpu):
    idx = cpu['cpu_id']

    # If there are multiple cpus, we have to match back the cpu_id
    def scalar(stat): return self.getCPUScalarStat(stat, idx)
    def vector(stat): return self.getCPUVectorStat(stat, idx)

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
    commitInsts = scalar("commit.committedInsts")
    commitIntInsts = scalar("commit.committedIntInsts") * self.int_inst_multiplier
    commitFpInsts = scalar("commit.committedFpInsts")
    commitInsts = commitIntInsts + commitFpInsts
    commitCalls = scalar("commit.committedCallInsts")

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

    # dtlb.
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

    # itlb.
    reads = scalar("itb.rdAccesses")
    readMisses = scalar("itb.rdMisses")
    writes = scalar("itb.wrAccesses")
    writeMisses = scalar("itb.wrMisses")
    core.itlb.total_accesses = reads + writes
    core.itlb.total_misses = readMisses + writeMisses

    # L1DCache
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
