
import Gem5Stats

import Constants as C

import os


class CactiCacheEnergy:
    def __init__(self, fn):
        with open(fn) as f:
            line0 = f.readline()
            fields = line0.split(', ')
            line1 = f.readline()
            values = line1.split(', ')
            # print(values)
            self.banks = float(values[2])
            self.readNJ = float(values[8])
            self.writeNJ = float(values[9])
            self.leakPerBackMW = float(values[10])
            self.leakMW = self.banks * self.leakPerBackMW
            self.areaMM2 = float(values[11])


class HWPrefetchEnergy:
    def __init__(self, stats):
        # Get the cacti results.
        self.pc_table = CactiCacheEnergy(os.path.join(
            C.LLVM_TDG_DIR, 'benchmark', 'Utils', 'hw-prefetch.cfg.out'))

        seconds = stats.get_sim_seconds()
        self.pc_table_leak_J = self.pc_table.leakMW * seconds / 1000


        # L1
        l1_uses = stats.get_default(
                'system.cpu.dcache.overall_accesses::total', 0)
        self.pc_table_dynamic_NJ = self.pc_table.readNJ * \
            l1_uses + self.pc_table.writeNJ * l1_uses * 0.5 

        # L3
        l2_uses = stats.get_default(
                'system.cpu.l1_5dcache.overall_accesses::total', 0)
        self.pc_table_dynamic_NJ += self.pc_table.readNJ * \
            l2_uses + self.pc_table.writeNJ * l2_uses * 0.5 

        # L3
        l3_uses = stats.get_default(
                'system.l2.overall_accesses::total', 0)
        self.pc_table_dynamic_NJ += self.pc_table.readNJ * \
            l3_uses + self.pc_table.writeNJ * l3_uses * 0.5 

    def get_hw_prefetch_energy(self):
        total_leak = self.pc_table_leak_J
        total_dynamic = self.pc_table_dynamic_NJ 
        total_dynamic /= 1e9
        return total_dynamic + total_leak

