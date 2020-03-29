
def setStatsDTLB(self, tlb, cpuId):
    def scalar(stat): return self.getScalarStats(tlb.path + '.' + stat)
    reads = scalar("rdAccesses")
    readMisses = scalar("rdMisses")
    writes = scalar("wrAccesses")
    writeMisses = scalar("wrMisses")
    core = self.xml.sys.core[cpuId]
    core.dtlb.total_accesses = reads + writes
    core.dtlb.read_accesses = reads
    core.dtlb.write_accesses = writes
    core.dtlb.read_misses = readMisses
    core.dtlb.write_misses = writeMisses
    core.dtlb.total_misses = readMisses + writeMisses

def setStatsITLB(self, tlb, cpuId):
    def scalar(stat): return self.getScalarStats(tlb.path + '.' + stat)
    reads = scalar("rdAccesses")
    readMisses = scalar("rdMisses")
    writes = scalar("wrAccesses")
    writeMisses = scalar("wrMisses")
    core = self.xml.sys.core[cpuId]
    core.itlb.total_accesses = reads + writes
    core.itlb.total_misses = readMisses + writeMisses
  