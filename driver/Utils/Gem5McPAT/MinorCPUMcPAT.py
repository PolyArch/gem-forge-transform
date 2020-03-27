
def configureMinorCPU(self, cpu):
    idx = cpu.cpu_id
    core = self.xml.sys.core[idx]
    core.clock_rate = self.toMHz(self.getCPUClockDomain())
    isaType = cpu.isa[0]['type']
    core.x86 = isaType == 'X86ISA'
    core.machine_type = 1; # 1 for inorder.

    core.fetch_width = cpu.fetch2InputBufferSize
    core.decode_width = cpu.decodeInputWidth
    # Make the peak issueWidth the same as issueWidth.
    core.issue_width = cpu.executeIssueLimit
    core.peak_issue_width = cpu.executeIssueLimit
    core.commit_width = cpu.executeCommitLimit

    # There is no float issue width in gem5.
    # Make it min(issueWidth, numFPU).
    core.fp_issue_width = min(cpu.executeIssueLimit, 6)

    # Integer pipeline and float pipeline depth.
    intExe = 3
    fpExe = 6
    if isaType == 'X86ISA':
      intExe = 2
      fpExe = 8
    elif isaType == 'ArmISA':
      intExe = 3
      fpExe = 7
    baseStages = cpu.fetch1ToFetch2ForwardDelay + \
        cpu.fetch2ToDecodeForwardDelay + \
        cpu.decodeToExecuteForwardDelay + \
        3 # Simply issue execute commit stages

    # ! PyBind will copy it.
    pipeline_depth = core.pipeline_depth
    pipeline_depth[0] = intExe + baseStages
    pipeline_depth[1] = fpExe + baseStages
    core.pipeline_depth = pipeline_depth

    # Buffer between the fetch and decode stage.
    # ! Multiply by 4 here, otherwise cacti complains it's too small and fails.
    core.instruction_buffer_size = cpu.fetch2InputBufferSize * 4

    # Again gem5 does not distinguish int/fp instruction window.
    core.instruction_window_size = cpu.executeInputBufferSize
    core.fp_instruction_window_size = cpu.executeInputBufferSize

    core.ROB_size = cpu.executeInputBufferSize
    # Arch register.
    archIntRegs = 32
    archFpRegs = 128 # Vectorization?
    core.archi_Regs_IRF_size = archIntRegs
    core.archi_Regs_FRF_size = archFpRegs
    core.phy_Regs_IRF_size = archIntRegs
    core.phy_Regs_FRF_size = archFpRegs
    core.store_buffer_size = cpu.executeLSQStoreBufferSize
    # Not exactly right
    core.load_buffer_size = cpu.executeLSQTransfersQueueSize

    # X86 ONLY
    if isaType == 'X86ISA':
        core.opt_local = 0
        core.instruction_length = 32
        core.opcode_width = 16
        core.micro_opcode_width = 8

    # Instruction TLB.
    try:
        mcpatITLB = core.itlb
        mcpatITLB.number_entries = self.getTLBSize(cpu.itb)
    except AttributeError:
        print('Warn! Failed to configure ITLB.')

    # Data TLB.
    try:
        mcpatDTLB = core.dtlb
        mcpatDTLB.number_entries = self.getTLBSize(cpu.dtb)
    except AttributeError:
        print('Warn! Failed to configure DTLB.')

    # L1 directory.
    mcpatL1Directory = self.xml.sys.L1Directory[idx]
    mcpatL1Directory.clockrate = self.toMHz(self.getCPUClockDomain())

def setStatsMinorCPU(self, cpu):
    core = self.xml.sys.core[cpu.cpu_id]
    def scalar(stat): return self.getScalarStats(cpu.path + '.' + stat)
    def vector(stat): return self.getVecStatsTotal(cpu.path + '.' + stat)
    def op(stat): return self.getScalarStats(cpu.path + '.op_class_0::' + stat)

    decodedInsts = scalar("decode.ops")
    branchInsts = scalar("fetch2.branches")
    loadInsts = scalar("lsq.loads")
    storeInsts = scalar("lsq.stores")
    commitInsts = scalar("committedOps")
    commitIntInsts = scalar("commit.intOps")
    commitFpInsts = scalar("commit.fpOps")
    totalCycles = scalar("numCycles")
    idleCycles = scalar("idleCycles")
    robReads = 0
    robWrites = 0

    # Gem5 seems not distinguish rename int/fp operands.
    # Just make rename float writes 0.
    renameWrites = 0
    renameReads = 0 
    renameFpReads = 0
    renameFpWrites = 0

    instWinReads = scalar("execute.iqIntReads")
    instWinWrites = scalar("execute.iqIntWrites")
    instWinWakeUpAccesses = scalar("execute.iqIntWakeups")
    instWinFpReads = scalar("execute.iqFpReads")
    instWinFpWrites = scalar("execute.iqFpWrites")
    instWinFpWakeUpAccesses = scalar("execute.iqFpWakeups")

    intRegReads = scalar("execute.intRegReads")
    intRegWrites = scalar("execute.intRegWrites")
    fpRegReads = scalar("execute.fpRegReads")
    fpRegWrites = scalar("execute.fpRegWrites")

    commitCalls = scalar("commit.callInsts")

    intALUOps = [
        'IntAlu',
        'SimdAdd',
        'SimdAddAcc',
        'SimdAlu',
        'SimdCmp',
        'SimdCvt',
        'SimdMisc',
        'SimdShift',
        'SimdShiftAcc',
        'SimdReduceAdd',
        'SimdReduceAlu',
        'SimdReduceCmp',
    ]
    intMultAndDivOps = [
        'IntMult',
        'IntDiv',
        'SimdMult',
        'SimdMultAcc',
        'SimdDiv',
        'SimdSqrt',
    ]
    fpOps = [
        'FloatAdd',
        'FloatCmp',
        'FloatCvt',
        'FloatMult',
        'FloatMultAcc',
        'FloatDiv',
        'FloatMisc',
        'FloatSqrt',
        'SimdFloatAdd',
        'SimdFloatAlu',
        'SimdFloatCmp',
        'SimdFloatCvt',
        'SimdFloatMult',
        'SimdFloatMultAcc',
        'SimdFloatDiv',
        'SimdFloatMisc',
        'SimdFloatSqrt',
        'SimdFloatReduceAdd',
        'SimdFloatReduceCmp',
    ]
    intALU = sum([op(o) for o in intALUOps])
    fpALU = sum([op(o) for o in fpOps])
    multiAndDiv = sum([op(o) for o in intMultAndDivOps])
    print('IntALUOps {x} IntMultDivOps {y} FpOps {z}'.format(
        x=intALU, y=multiAndDiv, z=fpALU
    ))

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

    core.inst_window_reads = instWinReads
    core.inst_window_writes = instWinWrites
    core.inst_window_wakeup_accesses = instWinWakeUpAccesses
    core.inst_window_selections = 0
    core.fp_inst_window_reads = instWinFpReads
    core.fp_inst_window_writes = instWinFpWrites
    core.fp_inst_window_wakeup_accesses = instWinFpWakeUpAccesses
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
    core.ialu_accesses = intALU
    core.fpu_accesses = fpALU
    core.mul_accesses = multiAndDiv
    core.cdb_alu_accesses = intALU
    core.cdb_mul_accesses = multiAndDiv
    core.cdb_fpu_accesses = fpALU
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
