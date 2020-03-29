
class Gem5Stats:
    def __init__(self, benchmark, fn, lines=None):
        self.benchmark = benchmark
        self.fn = fn
        self.stats = dict()
        if lines is None:
            stats = open(self.fn, 'r')
        else:
            stats = lines
        for line in stats:
            if len(line) == 0:
                continue
            if line.find('End Simulation Statistics') != -1:
                break
            if line[0] == '-':
                continue
            fields = line.split()
            try:
                self.stats[fields[0]] = float(fields[1])
            except Exception as e:
                pass
                # print('ignore line {line}'.format(line=line))
        if lines is None:
            # This is a file.
            stats.close()

    def merge(self, other):
        for stat_id in other.stats:
            if stat_id in self.stats:
                self.stats[stat_id] += other.stats[stat_id]
            else:
                self.stats[stat_id] = other.stats[stat_id]

    def get_default(self, key, default):
        if key in self.stats:
            return self.stats[key]
        else:
            return default

    def get_sim_seconds(self):
        return self.get_cpu_cycles()
        # return self['sim_seconds']

    def get_cpu_cycles(self):
        # Take the first cpu as the standard.
        if 'system.cpu.numCycles' in self:
            return self['system.cpu.numCycles']
        else:
            return self['system.cpu0.numCycles']

    def get_cpu_insts(self):
        if 'system.cpu.commit.committedInsts' in self:
            return self['system.cpu.committedInsts']
        else:
            return self['system.cpu0.committedInsts']

    """
    Region stats has not total.
    """

    def get_region_mem_access(self):
        return self.get_default('system.cpu.dcache.overall_accesses', 0)

    def get_region_l1_misses(self):
        return self.get_default('system.cpu.dcache.overall_misses', 0)

    def get_region_l2_misses(self):
        return self.get_default('system.l2.overall_misses', 0)

    def get_mem_access(self):
        return self.get_default('system.cpu.dcache.overall_accesses::total', 0)

    def get_l1_misses(self):
        return self.get_default('system.cpu.dcache.overall_misses::total', 0)

    def get_l2_misses(self):
        return self.get_default('system.l2.overall_misses::total', 0)

    def get_branches(self):
        return self.get_default('system.cpu.fetch.branchInsts', 0)

    def get_branch_misses(self):
        return self.get_default('system.cpu.fetch.branchPredMisses', 0)

    def __getitem__(self, key):
        try:
            return self.stats[key]
        except Exception as e:
            print('Failed to get {key} from {file}'.format(
                key=key,
                file=self.fn
            ))
            raise e
    def __contains__(self, key):
        return key in self.stats
