import prettytable


class Access:
    def __init__(self, line):
        if line[-1] == '\n':
            line = line[0:-1]
        fields = line.split(' ')
        assert(len(fields) >= 9)
        self.loop = fields[0]
        self.inst = fields[1]
        self.pattern = fields[2]
        self.acc_pattern = fields[3]
        self.iters = float(fields[4])
        self.accesses = float(fields[5])
        self.updates = float(fields[6])
        self.streams = float(fields[7])
        self.base_load = fields[8]

    def merge(self, other):
        assert(self.inst == other.inst)
        assert(self.loop == other.loop)
        assert(self.base_load == other.base_load)
        self.iters += other.iters
        self.accesses += other.accesses
        self.updates += other.updates
        self.streams += other.streams

    @staticmethod
    def print_table(selves):
        table = prettytable.PrettyTable([
            'Loop',
            'Inst',
            'Addr Pat',
            'Acc Pat',
            'Iters',
            'Accesses',
            'Updates',
            'Stream',
            'Base Load',
        ])
        selves.sort(reverse=True)
        for access in selves:
            table.add_row([
                access.loop,
                access.inst,
                access.pattern,
                access.acc_pattern,
                access.iters,
                access.accesses,
                access.updates,
                access.streams,
                access.base_load,
            ])
        print(table)

    def __str__(self):
        return '{loop} {inst} {pattern} {acc_pattern} {iters} {accesses} {updates} {streams} {base_load}'.format(
            loop=self.loop,
            inst=self.inst,
            pattern=self.pattern,
            acc_pattern=self.acc_pattern,
            iters=self.iters,
            accesses=self.accesses,
            updates=self.updates,
            streams=self.streams,
            base_load=self.base_load,
        )

    def __gt__(self, other):
        if self.accesses > other.accesses:
            return True
        elif self.accesses < other.accesses:
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
        assert(access.inst not in self.accesses)
        self.accesses[access.inst] = access

    def merge(self, other):
        for stat in self.stats:
            assert(stat in other.stats)
            self.stats[stat] += other.stats[stat]
        for inst in other.accesses:
            if inst in self.accesses:
                self.accesses[inst].merge(other.accesses[inst])
            else:
                self.accesses[inst] = other.accesses[inst]

    def print_stats(self):
        print('Constant Stream: {0:.4f}'.format((self.stats['ConstantCount'] /
                                                 self.stats['DynMemInstCount'])))

        print('Linear Stream: {0:.4f}'.format((self.stats['LinearCount'] /
                                               self.stats['DynMemInstCount'])))

        print('Quardric Stream: {0:.4f}'.format((self.stats['QuardricCount'] /
                                                 self.stats['DynMemInstCount'])))

        print('Indirect Stream: {0:.4f}'.format((self.stats['IndirectCount'] /
                                                 self.stats['DynMemInstCount'])))

        print('AddRec Stream: {0:.4f}'.format(((self.stats['AddRecLoadCount'] + self.stats['AddRecStoreCount']) /
                                               self.stats['DynMemInstCount'])))

    def print_access(self):
        vals = list(self.accesses.values())
        Access.print_table(vals)
