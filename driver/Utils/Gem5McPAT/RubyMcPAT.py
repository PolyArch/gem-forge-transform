
"""
Just bunch of function to process Gem5's ruby cache system.
"""

def getCacheTagAccessLatency(cache):
    if cache['sequential_access']:
        return cache['data_latency'] + cache['tag_latency']
    else:
        return max(cache['data_latency'], cache['tag_latency'])

def getControllerIdx(cntrl):
    # We do not distinguish l0/l1/l2_cntrl
    idx = 0
    if len(cntrl.name) > len('l1_cntrl'):
        idx = int(cntrl.name[len('l1_cntrl'):])
    return idx

        
def configureL1ICache(self, cntrl):
    # L1 inst cache.
    idx = getControllerIdx(cntrl)
    mcpatCPU = self.xml.sys.core[idx]
    mcpatL1I = mcpatCPU.icache
    cache = cntrl.Icache

    config = mcpatL1I.icache_config
    config[0] = cache.size
    config[1] = cache.replacement_policy.block_size
    config[2] = cache.assoc
    config[3] = 4 # Bank
    config[4] = 4 # Throughput
    config[5] = cache.tagAccessLatency
    config[6] = 32 # Output width
    config[7] = 0 # Cache policy: 0 for no-write or write-through, 1 for write-back
    mcpatL1I.icache_config = config

    # Ruby has no concept of MSHRs?
    buffer_sizes = mcpatL1I.buffer_sizes
    buffer_sizes[0] = 16 # MSHRs
    buffer_sizes[1] = 16 # MSHRs
    buffer_sizes[2] = 16 # MSHRs
    buffer_sizes[3] = 16 # Write buffers
    mcpatL1I.buffer_sizes = buffer_sizes

def configureL1DCache(self, cntrl):
    idx = getControllerIdx(cntrl)
    mcpatCPU = self.xml.sys.core[idx]
    mcpatL1D = mcpatCPU.dcache
    cache = cntrl.Dcache

    config = mcpatL1D.dcache_config
    config[0] = cache.size
    config[1] = cache.replacement_policy.block_size
    config[2] = cache.assoc
    config[3] = 4 # Bank
    config[4] = 4 # Throughput
    config[5] = cache.tagAccessLatency
    config[6] = 1 # What is this?
    mcpatL1D.dcache_config = config

    # Ruby has no concept of MSHRs?
    buffer_sizes = mcpatL1D.buffer_sizes
    buffer_sizes[0] = 16 # MSHRs
    buffer_sizes[1] = 16 # MSHRs
    buffer_sizes[2] = 16 # MSHRs
    buffer_sizes[3] = 16 # Write buffers
    mcpatL1D.buffer_sizes = buffer_sizes

def configureL1Cache(self, ruby):
    for cntrl in ruby.__dict__:
        if cntrl.startswith('l0_cntrl'):
            configureL1ICache(self, ruby.__dict__[cntrl])
            configureL1DCache(self, ruby.__dict__[cntrl])
            

def setStatsL1ICache(self, cntrl, isGemForgeCPU):
    idx = getControllerIdx(cntrl)
    def scalar(stat): return self.getScalarStats(cntrl.path + '.' + stat)
    mcpatCPU = self.xml.sys.core[idx]
    mcpatL1I = mcpatCPU.icache

    totalAccesses = scalar('Dcache.demand_accesses')
    totalMisses = scalar('Dcache.demand_misses')
    # ! Jesus no easy way to distinguish read/write requests,
    # ! Just take them as half.
    mcpatL1I.read_accesses = totalAccesses
    mcpatL1I.read_misses = totalMisses
    # * For LLVMTraceCPU, there is no instruction cache simulated,
    # * we added the number of instructions as an approximation.
    # if isGemForgeCPU:
    #     fetchedInsts = self.getCPUScalarStat("fetch.Insts", cpuId)
    #     mcpatL1I.read_accesses = fetchedInsts / 9
    #     mcpatL1I.read_misses = fetchedInsts / 1000

def setStatsL1DCache(self, cntrl):
    idx = getControllerIdx(cntrl)
    def scalar(stat): return self.getScalarStats(cntrl.path + '.' + stat)
    mcpatCPU = self.xml.sys.core[idx]
    mcpatL1D = mcpatCPU.dcache

    totalAccesses = scalar('Dcache.demand_accesses')
    totalMisses = scalar('Dcache.demand_misses')
    # ! Jesus no easy way to distinguish read/write requests,
    # ! Just take them as half.
    mcpatL1D.read_accesses = totalAccesses / 2
    mcpatL1D.write_accesses = totalAccesses / 2
    mcpatL1D.read_misses = totalMisses / 2
    mcpatL1D.write_misses = totalMisses / 2

def setStatsL1Cache(self, ruby):
    for cntrl in ruby.__dict__:
        if cntrl.startswith('l0_cntrl'):
            setStatsL1ICache(self, ruby.__dict__[cntrl], False)
            setStatsL1DCache(self, ruby.__dict__[cntrl])

def configureL2Directories(self, cache):
    L2DIdx = self.xml.sys.number_of_L2Directories
    assert(L2DIdx == 0)
    self.xml.sys.number_of_L2Directories += 1
    L2D = self.xml.sys.L2Directory[L2DIdx]
    L2D.clockrate = self.toMHz(self.getCPUClockDomain())

def configureL2CacheBank(self, cntrl):
    L2Idx = getControllerIdx(cntrl)
    print('Setting L2Bank {idx}'.format(idx=L2Idx))
    mcpatL2 = self.xml.sys.L2[L2Idx]
    L2cache = cntrl.cache

    L2_config = mcpatL2.L2_config
    L2_config[0] = L2cache.size
    L2_config[1] = L2cache.replacement_policy.block_size
    L2_config[2] = L2cache.assoc
    L2_config[3] = 4 # Bank
    L2_config[4] = 4 # Throughput
    L2_config[5] = L2cache.tagAccessLatency
    L2_config[6] = 1 # What is this?
    mcpatL2.L2_config = L2_config

    # Ruby has no concept of MSHRs?
    buffer_sizes = mcpatL2.buffer_sizes
    buffer_sizes[0] = 16 # MSHRs
    buffer_sizes[1] = 16 # MSHRs
    buffer_sizes[2] = 16 # MSHRs
    buffer_sizes[3] = 16 # Write buffers
    mcpatL2.buffer_sizes = buffer_sizes

    # High performance device type.
    mcpatL2.device_type = 0
    ports = mcpatL2.ports
    ports[0] = 1 # Reads
    ports[1] = 1 # Writes
    ports[1] = 1 # Read-Writes
    mcpatL2.ports = ports 

    mcpatL2.clockrate = self.toMHz(self.getCPUClockDomain())

def configureL2Cache(self, ruby):
    # Count the number of L2 controllers.
    # ! In MESI_Three_Level, L0 - L1 - L2
    numL2s = 0
    for cntrl in ruby.__dict__:
        if cntrl.startswith('l1_cntrl'):
            configureL2CacheBank(self, ruby.__dict__[cntrl])
            numL2s += 1
    self.xml.sys.number_of_L2s = numL2s
    print('Number of L2s {x}'.format(x = self.xml.sys.number_of_L2s))

def setStatsL2CacheBank(self, cntrl):
    L2Idx = getControllerIdx(cntrl)
    def scalar(stat): return self.getScalarStats(cntrl.path + '.' + stat)
    mcpatL2 = self.xml.sys.L2[L2Idx]

    totalAccesses = scalar('cache.demand_accesses')
    totalMisses = scalar('cache.demand_misses')
    # ! Jesus no easy way to distinguish read/write requests,
    # ! Just take them as half.
    mcpatL2.read_accesses = totalAccesses / 2
    mcpatL2.write_accesses = totalAccesses / 2
    mcpatL2.read_misses = totalMisses / 2
    mcpatL2.write_misses = totalMisses / 2

def setStatsL2Cache(self, ruby):
    for cntrl in ruby.__dict__:
        if cntrl.startswith('l1_cntrl'):
            setStatsL2CacheBank(self, ruby.__dict__[cntrl])

def configureL3CacheBank(self, cntrl):
    L3Idx = getControllerIdx(cntrl)
    print('Setting L3Bank {idx}'.format(idx=L3Idx))
    mcpatL3 = self.xml.sys.L3[L3Idx]
    L3cache = cntrl.L2cache

    L3_config = mcpatL3.L3_config
    L3_config[0] = L3cache.size
    L3_config[1] = L3cache.replacement_policy.block_size
    L3_config[2] = L3cache.assoc
    L3_config[3] = 4 # Bank
    L3_config[4] = 4 # Throughput
    L3_config[5] = L3cache.tagAccessLatency
    L3_config[6] = 1 # What is this?
    mcpatL3.L3_config = L3_config

    # Ruby has no concept of MSHRs?
    buffer_sizes = mcpatL3.buffer_sizes
    buffer_sizes[0] = 16 # MSHRs
    buffer_sizes[1] = 16 # MSHRs
    buffer_sizes[2] = 16 # MSHRs
    buffer_sizes[3] = 16 # Write buffers
    mcpatL3.buffer_sizes = buffer_sizes

    # High performance device type.
    mcpatL3.device_type = 0
    ports = mcpatL3.ports
    ports[0] = 1 # Reads
    ports[1] = 1 # Writes
    ports[1] = 1 # Read-Writes
    mcpatL3.ports = ports 

    mcpatL3.clockrate = self.toMHz(self.getCPUClockDomain())

def configureNoC(self, ruby, numL3s):
    print('Number of NoCs {x}'.format(x=self.xml.sys.number_of_NoCs))
    mcpatNoC = self.xml.sys.NoC[0]
    mcpatNoC.clockrate = self.toMHz(self.getCPUClockDomain())
    mcpatNoC.type = 1 # 0 bus, 1 NoC.

    network = ruby.network
    mcpatNoC.horizontal_nodes = numL3s / network.num_rows
    mcpatNoC.vertical_nodes = network.num_rows
    mcpatNoC.has_global_link = 0
    mcpatNoC.link_throughput = 1
    mcpatNoC.link_latency = network.int_links[0].latency
    mcpatNoC.input_ports = 5
    mcpatNoC.output_ports = 5
    mcpatNoC.virtual_channel_per_port = \
        network.routers[0].vcs_per_vnet
    print(network.ni_flit_size)
    print(network.buffers_per_data_vc)
    mcpatNoC.flit_bits = network.ni_flit_size * 8
    mcpatNoC.input_buffer_entries_per_vc = network.buffers_per_data_vc
    mcpatNoC.chip_coverage = 1

def configureL3Cache(self, ruby):
    # Count the number of L2 controllers.
    # ! In MESI_Three_Level, L0 - L1 - L2
    numL3s = 0
    for cntrl in ruby.__dict__:
        if cntrl.startswith('l2_cntrl'):
            configureL3CacheBank(self, ruby.__dict__[cntrl])
            numL3s += 1
    self.xml.sys.number_of_L3s = numL3s
    print('Number of L3s {x}'.format(x = self.xml.sys.number_of_L3s))
    configureNoC(self, ruby, numL3s)

def setStatsL3CacheBank(self, cntrl):
    L3Idx = getControllerIdx(cntrl)
    def scalar(stat): return self.getScalarStats(cntrl.path + '.' + stat)
    mcpatL3 = self.xml.sys.L3[L3Idx]

    totalAccesses = scalar('L2cache.demand_accesses')
    totalMisses = scalar('L2cache.demand_misses')
    # ! Jesus no easy way to distinguish read/write requests,
    # ! Just take them as half.
    mcpatL3.read_accesses = totalAccesses / 2
    mcpatL3.write_accesses = totalAccesses / 2
    mcpatL3.read_misses = totalMisses / 2
    mcpatL3.write_misses = totalMisses / 2

    print(totalAccesses)

def setStatsNoC(self, ruby):
    mcpatNoC = self.xml.sys.NoC[0]
    mcpatNoC.total_accesses = self.getScalarStats('system.ruby.network.total_hops')

def setStatsL3Cache(self, ruby):
    numL3s = self.xml.sys.number_of_L3s
    for cntrl in ruby.__dict__:
        if cntrl.startswith('l2_cntrl'):
            setStatsL3CacheBank(self, ruby.__dict__[cntrl])
            numL3s += 1
    setStatsNoC(self, ruby)
