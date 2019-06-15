
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


class StreamEngineEnergy:
    def __init__(self, stats):
        # Get the cacti results.
        self.se_fifo = CactiCacheEnergy(os.path.join(
            C.LLVM_TDG_DRIVER_DIR, 'Utils', 'se-fifo.cfg.out'))
        self.se_table = CactiCacheEnergy(os.path.join(
            C.LLVM_TDG_DRIVER_DIR, 'Utils', 'se-table.cfg.out'))
        self.se_unit = CactiCacheEnergy(os.path.join(
            C.LLVM_TDG_DRIVER_DIR, 'Utils', 'se-unit.cfg.out'))

        seconds = stats.get_sim_seconds()
        self.se_fifo_leak_J = self.se_fifo.leakMW * seconds / 1000
        self.se_table_leak_J = self.se_table.leakMW * seconds / 1000
        self.se_unit_leak_J = self.se_unit.leakMW * seconds / 1000

        stream_uses = stats.get_default(
            'system.cpu.accs.stream.numLoadElementsUsed', 0)
        stream_elements = stats.get_default(
            'system.cpu.accs.stream.numElementsAllocated', 0)
        stream_stepped = stats.get_default(
            'syste.cpu.accs.stream.numStepped', 0)
        self.se_fifo_dynamic_NJ = self.se_fifo.readNJ * \
            stream_uses + self.se_fifo.writeNJ * stream_elements
        self.se_table_dynamic_NJ = self.se_table.readNJ * \
            stream_uses + self.se_table.writeNJ * stream_stepped
        self.se_unit_dynamic_NJ = self.se_unit.readNJ * \
            stream_elements + self.se_unit.writeNJ * stream_elements

    def get_se_energy(self):
        total_leak = self.se_fifo_leak_J + self.se_table_leak_J + self.se_unit_leak_J
        total_dynamic = self.se_fifo_dynamic_NJ + \
            self.se_table_dynamic_NJ + self.se_unit_dynamic_NJ
        total_dynamic /= 1e9
        return total_dynamic + total_leak
