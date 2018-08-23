import prettytable


class Access:
    def __init__(self, line):
        if line[-1] == '\n':
            line = line[0:-1]
        fields = line.split(' ')
        assert(len(fields) >= 12)
        self.loop = fields[0]
        self.inst = fields[1]
        self.pattern = fields[2]
        self.acc_pattern = fields[3]
        self.iters = float(fields[4])
        self.accesses = float(fields[5])
        self.updates = float(fields[6])
        self.streams = float(fields[7])
        self.base_load = int(fields[8])
        self.footprint = int(fields[9])
        self.addr_insts = int(fields[10])
        self.chosen = fields[11]

    def merge(self, other):
        assert(self.inst == other.inst)
        assert(self.loop == other.loop)
        assert(self.base_load == other.base_load)
        assert(self.addr_insts == other.addr_insts)
        assert(self.chosen == other.chosen)
        self.iters += other.iters
        self.accesses += other.accesses
        self.updates += other.updates
        self.streams += other.streams
        self.footprint += other.footprint

    @staticmethod
    def get_fields():
        return [
            'Loop',
            'Inst',
            'Addr Pat',
            'Acc Pat',
            'Iters',
            'Accesses',
            'Updates',
            'Stream',
            'BaseLoads',
            'Footprint',
            'AddrInsts',
            'Chosen',
        ]

    def get_row(self):
        return [
            self.loop,
            self.inst,
            self.pattern,
            self.acc_pattern,
            self.iters,
            self.accesses,
            self.updates,
            self.streams,
            self.base_load,
            self.footprint,
            self.addr_insts,
            self.chosen,
        ]

    @staticmethod
    def print_table(selves):
        table = prettytable.PrettyTable(Access.get_fields())
        table.align = 'r'
        selves.sort(reverse=True)
        for access in selves:
            table.add_row(access.get_row())
        print(table)

    @staticmethod
    def dump_csv(selves, fn):
        import csv
        selves.sort(reverse=True)
        with open(fn, 'wb') as csvfile:
            writer = csv.writer(csvfile)
            writer.writerow(Access.get_fields())
            for access in selves:
                writer.writerow(access.get_row())

    def __gt__(self, other):
        if self.accesses > other.accesses:
            return True
        elif self.accesses < other.accesses:
            return False

        if self.inst > other.inst:
            return True
        elif self.inst < other.inst:
            return False

        if self.iters > other.iters:
            return True
        elif self.iters < other.iters:
            return False

        return self.streams < other.streams


class StreamStatistics:
    def __init__(self, files):
        if isinstance(files, list):
            self.parse(files[0])
            if len(files) > 1:
                self.merge(StreamStatistics(files[1:]))
        else:
            self.parse(files)

    def parse(self, file):
        self.stats = dict()
        self.accesses = dict()
        with open(file) as f:
            for line in f:
                if line.startswith('----'):
                    # Ignore the header line.
                    continue
                if '::' in line:
                    self.parse_access(line)
                else:
                    self.parse_stat(line)

    def parse_stat(self, line):
        fields = line.split(': ')
        assert(len(fields) == 2)
        self.stats[fields[0]] = float(fields[1])

    def parse_access(self, line):
        access = Access(line)
        uid = access.inst + access.loop
        assert(uid not in self.accesses)
        self.accesses[uid] = access

    def merge(self, other):
        for stat in self.stats:
            assert(stat in other.stats)
            self.stats[stat] += other.stats[stat]
        for inst in other.accesses:
            if inst in self.accesses:
                self.accesses[inst].merge(other.accesses[inst])
            else:
                self.accesses[inst] = other.accesses[inst]

    def calculate_indirect(self):
        indirect = dict()
        for access in self.accesses.values():
            # If this stream is chosen, pick it.
            if access.chosen == 'YES':
                indirect[access.inst] = access
                continue
            # If not chosen, pick the lowerest level of loop.
            if access.inst in indirect:
                current_indirect = indirect[access.inst]
                if current_indirect.chosen == 'YES':
                    continue
                elif access.streams > current_indirect.streams:
                    indirect[access.inst] = access
            else:
                indirect[access.inst] = access

        indirect_accesses = 0
        indirect_baseloads = 0
        indirect_addr_insts = 0
        for access in indirect.values():
            if access.base_load < 1:
                # This is not an indirect stream
                continue
            indirect_accesses += access.accesses
            indirect_baseloads += access.base_load * access.accesses
            indirect_addr_insts += access.addr_insts * access.accesses
        if indirect_accesses != 0:
            indirect_baseloads /= indirect_accesses
            indirect_addr_insts /= indirect_accesses
        return (indirect_accesses, indirect_baseloads, indirect_addr_insts)

    def calculate_footprint(self):
        visited = set()
        accesses = 0
        footprint_weighted = 0
        footprint = 0
        for access in self.accesses.values():
            if access.inst in visited:
                continue
            if access.footprint == 0:
                continue
            visited.add(access.inst)
            footprint_weighted += access.footprint * access.accesses
            footprint += access.footprint
            accesses += access.accesses
        footprint_weighted /= accesses
        reuse = accesses / float(footprint)
        return (footprint_weighted, reuse)

    def print_stats(self):
        table = prettytable.PrettyTable([
            'ConstStream',
            'LinearStream',
            'QuardricStream',
            'Indirect',
            'AvgIndirectBase',
            'AvgIndirectAddr',
            'ChosenIndirect',
            'ChosenStream',
            'RemovedAddrInsts',
            'Footprint',
            'Reuse',
        ])
        table.float_format = '.4'
        const_stream = (self.stats['ConstantCount'] /
                        self.stats['DynMemInstCount'])

        linear_stream = (self.stats['LinearCount'] /
                         self.stats['DynMemInstCount'])

        quardric_stream = (self.stats['QuardricCount'] /
                           self.stats['DynMemInstCount'])

        chosen_indirect_stream = (self.stats['IndirectCount'] /
                                  self.stats['DynMemInstCount'])

        add_rec_stream = ((self.stats['AddRecLoadCount'] + self.stats['AddRecStoreCount']) /
                          self.stats['DynMemInstCount'])

        removed_addr_insts = (self.stats['RemovedAddrInstCount'] /
                              self.stats['TracedDynInstCount'])

        indirect_accesses, indirect_baseloads, indirect_addr_insts = self.calculate_indirect()

        footprint, reuse = self.calculate_footprint()

        table.add_row([
            const_stream,
            linear_stream,
            quardric_stream,
            indirect_accesses / self.stats['DynMemInstCount'],
            indirect_baseloads,
            indirect_addr_insts,
            chosen_indirect_stream,
            const_stream + linear_stream + quardric_stream + chosen_indirect_stream,
            removed_addr_insts,
            footprint,
            reuse,
        ])
        print(table)

    def print_access(self):
        vals = list(self.accesses.values())
        Access.print_table(vals)

    def dump_csv(self, fn):
        vals = list(self.accesses.values())
        Access.dump_csv(vals, fn)
