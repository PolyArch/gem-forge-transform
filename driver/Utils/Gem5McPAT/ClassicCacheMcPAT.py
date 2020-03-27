
"""
Just bunch of function to process Gem5's classic cache system.
"""

def getCacheTagAccessLatency(cache):
    if cache['sequential_access']:
        return cache['data_latency'] + cache['tag_latency']
    else:
        return max(cache['data_latency'], cache['tag_latency'])

def configureL1ICache(self, cpuId, instL1):
    # L1 inst cache.
    mcpatCPU = self.xml.sys.core[cpuId]
    instL1Tag = instL1['tags']
    mcpatInstL1 = mcpatCPU.icache
    accessLatency = getCacheTagAccessLatency(instL1)
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
    mcpatInstL1.buffer_sizes[3] = 0                # wb_buffer_size

def setStatsL1ICache(self, cpuId, instL1, isGemForgeCPU):
    def vector(stat):
        return self.getVecStatsTotal(instL1['path'] + '.' + stat)

    reads = vector("ReadReq_accesses")
    readMisses = vector("ReadReq_misses")
    mcpatCPU = self.xml.sys.core[cpuId]
    mcpatInstL1 = mcpatCPU.icache
    mcpatInstL1.read_accesses = reads
    mcpatInstL1.read_misses = readMisses
    # * For LLVMTraceCPU, there is no instruction cache simulated,
    # * we added the number of instructions as an approximation.
    if isGemForgeCPU:
        fetchedInsts = self.getCPUScalarStat("fetch.Insts", cpuId)
        mcpatInstL1.read_accesses = fetchedInsts / 9
        mcpatInstL1.read_misses = fetchedInsts / 1000


def configureL1DCache(self, cpuId, dataL1):
    mcpatCPU = self.xml.sys.core[cpuId]
    dataL1Tag = dataL1['tags']
    mcpatDataL1 = mcpatCPU.dcache
    accessLatency = getCacheTagAccessLatency(dataL1)
    mcpatDataL1.dcache_config[0] = dataL1['size']          # capacity
    # block_width
    mcpatDataL1.dcache_config[1] = dataL1Tag['block_size']
    mcpatDataL1.dcache_config[2] = dataL1['assoc']
    mcpatDataL1.dcache_config[3] = 1                           # bank
    mcpatDataL1.dcache_config[4] = accessLatency               # throughput
    mcpatDataL1.dcache_config[5] = accessLatency               # latency
    # output_width
    mcpatDataL1.dcache_config[6] = 32
    # cache_policy
    mcpatDataL1.dcache_config[7] = 0
    mcpatDataL1.buffer_sizes[0] = dataL1['mshrs']  # MSHR
    mcpatDataL1.buffer_sizes[1] = dataL1['mshrs']  # fill_buffer_size
    mcpatDataL1.buffer_sizes[2] = dataL1['mshrs']  # prefetch_buffer_size
    mcpatDataL1.buffer_sizes[3] = 0                # wb_buffer_size

def setStatsL1DCache(self, cpuId, dataL1):
    def vector(stat):
        return self.getVecStatsTotal(dataL1['path'] + '.' + stat)
    writes = vector("WriteReq_accesses")
    writeMisses = vector("WriteReq_misses")
    reads = vector("ReadReq_accesses")
    readMisses = vector("ReadReq_misses")
    mcpatCPU = self.xml.sys.core[cpuId]
    mcpatDataL1 = mcpatCPU.dcache
    mcpatDataL1.read_accesses = reads
    mcpatDataL1.write_accesses = writes
    mcpatDataL1.read_misses = readMisses
    mcpatDataL1.write_misses = writeMisses

def configureL2Directories(self, cache):
    L2DIdx = self.xml.sys.number_of_L2Directories
    assert(L2DIdx == 0)
    self.xml.sys.number_of_L2Directories += 1
    L2D = self.xml.sys.L2Directory[L2DIdx]
    L2D.clockrate = self.toMHz(self.getCPUClockDomain())

def configureL2Cache(self, cpuId, cache):
    tags = cache['tags']
    assert(cpuId < self.xml.sys.number_of_L2s)
    L2 = self.xml.sys.L2[cpuId]
    L2.L2_config[0] = cache['size']
    L2.L2_config[1] = tags['block_size']
    L2.L2_config[2] = cache['assoc']
    L2.L2_config[3] = 1
    accessLatency = getCacheTagAccessLatency(cache)
    L2.L2_config[4] = accessLatency
    L2.L2_config[5] = accessLatency
    L2.L2_config[6] = 32
    L2.L2_config[7] = 1

    L2.buffer_sizes[0] = cache['mshrs']
    L2.buffer_sizes[1] = cache['mshrs']
    L2.buffer_sizes[2] = cache['mshrs']
    L2.buffer_sizes[3] = cache['write_buffers']
    L2.clockrate = self.toMHz(self.getCPUClockDomain())

def setStatsL2Cache(self, cpuId, cache):
    def vector(stat):
        return self.getVecStatsTotal(cache['path'] + '.' + stat)
    writes = vector("WritebackDirty_accesses") + \
        vector("WritebackClean_accesses")
    writeHits = vector("WritebackDirty_hits") + \
        vector("WritebackClean_hits")
    writeMisses = writes - writeHits
    reads = vector("ReadReq_accesses") + \
        vector("ReadExReq_accesses") + \
        vector("ReadSharedReq_accesses")
    readMisses = vector("ReadReq_misses") + \
        vector("ReadExReq_misses") + \
        vector("ReadSharedReq_misses")
    L2 = self.xml.sys.L2[cpuId]
    L2.read_accesses = reads
    L2.write_accesses = writes
    L2.read_misses = readMisses
    L2.write_misses = writeMisses

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

    accessLatency = getCacheTagAccessLatency(cache)
    L3.L3_config[4] = accessLatency
    L3.L3_config[5] = accessLatency
    L3.L3_config[6] = 32
    L3.L3_config[7] = 1

    L3.buffer_sizes[0] = cache['mshrs']
    L3.buffer_sizes[1] = cache['mshrs']
    L3.buffer_sizes[2] = cache['mshrs']
    L3.buffer_sizes[3] = cache['write_buffers']

    L3.clockrate = self.toMHz(self.getCPUClockDomain())

def setStatsL3Cache(self):
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
