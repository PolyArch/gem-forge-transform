import prettytable


class Access:
    def __init__(self, line):
        if line[-1] == '\n':
            line = line[0:-1]
        fields = line.split(' ')
        if len(fields) < 13:
            print(line)
        assert(len(fields) >= 13)
        self.loop = fields[0]
        self.inst = fields[1]
        self.pattern = fields[2]
        self.acc_pattern = fields[3]
        self.iters = float(fields[4])
        self.accesses = float(fields[5])
        self.updates = float(fields[6])
        self.streams = float(fields[7])
        self.level = int(fields[8])
        self.base_load = int(fields[9])
        self.stream_class = fields[10]
        self.footprint = int(fields[11])
        self.addr_insts = int(fields[12])
        self.alias_insts = int(fields[13])
        self.chosen = fields[14]

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
            'LoopLevel',
            'Class',
            'Footprint',
            'AddrInsts',
            'AliasInsts',
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
            self.level,
            self.base_load,
            self.stream_class,
            self.footprint,
            self.addr_insts,
            self.alias_insts,
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

        return self.level > other.level


class StreamStatistics:
    def __init__(self, files):
        self.next = None
        if isinstance(files, list):
            self.parse(files[0])
            if len(files) > 1:
                self.next = StreamStatistics(files[1:])
        else:
            self.parse(files)

        self.group_by_loop_level()

    def group_by_loop_level(self):
        self.inst_to_loop_level = dict()
        for access in self.accesses.values():
            # Ignore the out of region accesses.
            if access.loop == 'UNKNOWN':
                continue
            if access.stream_class == 'NOT_STREAM':
                continue
            if access.inst not in self.inst_to_loop_level:
                self.inst_to_loop_level[access.inst] = list()
            self.inst_to_loop_level[access.inst].append(access)
        # Sort by the stream level.
        for inst in self.inst_to_loop_level:
            self.inst_to_loop_level[inst].sort(
                key=lambda a: a.level,
                reverse=False,
            )

    def parse(self, file):
        self.stats = dict()
        self.accesses = dict()
        section = 0
        with open(file) as f:
            for line in f:
                if line.startswith('----'):
                    # Move to the next section
                    section += 1
                    continue
                if section == 2:
                    self.parse_access(line)
                elif section == 1:
                    self.parse_stat(line)

    def parse_stat(self, line):
        fields = line.split(': ')
        assert(len(fields) == 2)
        self.stats[fields[0]] = float(fields[1])

    def parse_access(self, line):
        access = Access(line)
        uid = access.inst + '|' + access.loop
        assert(uid not in self.accesses)
        self.accesses[uid] = access

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
        if accesses != 0:
            footprint_weighted /= accesses
            reuse = accesses / float(footprint)
        else:
            reuse = 0.0
        return (footprint_weighted, reuse)

    def calculate_out_of_loop(self):
        total = 0
        for access in self.accesses.values():
            if access.stream_class == 'NOT_STREAM':
                total += access.accesses
        if self.next is not None:
            total += self.next.calculate_out_of_loop()
        return total

    def calculate_out_of_region(self):
        total = 0
        for access in self.accesses.values():
            if access.loop != 'UNKNOWN' and access.stream_class == 'NOT_STREAM':
                total += access.accesses
        return total

    def calculate_total_mem_accesses(self):
        if self.next is not None:
            return (self.stats['DynMemInstCount'] + self.next.calculate_total_mem_accesses())
        else:
            return self.stats['DynMemInstCount']

    def calculate_stream_breakdown(self, level):
        filtered = list()
        for inst in self.inst_to_loop_level:
            streams = self.inst_to_loop_level[inst]
            if len(streams) <= level:
                # This stream does not have enough deep level.
                continue
            filtered.append(streams[level])
        result = {
            'INDIRECT_CONTINUOUS': 0,
            'INCONTINUOUS': 0,
            'RECURSIVE': 0,
            'AFFINE': 0,
            'RANDOM': 0,
            'AFFINE_BASE': 0,
            'RANDOM_BASE': 0,
            'POINTER_CHASE': 0,
            'CHAIN_BASE': 0,
            'MULTI_BASE': 0,
        }
        for access in filtered:
            result[access.stream_class] += access.accesses
        if self.next is not None:
            next_result = self.next.calculate_stream_breakdown(level)
            for field in result:
                result[field] += next_result[field]
        return result

    def print_stream_breakdown(self):
        table = prettytable.PrettyTable([
            'OutOfLoop',
            'Recursive',
            'Incontinuous',
            'IndirectContinuous',
            'L0_AFFINE',
            'L0_RAMDOM',
            'L0_AFFINE_BASE',
            'L0_RANDOM_BASE',
            'L0_POINTER_CHASE',
            'L0_CHAIN_BASE',
            'L0_MULTI_BASE',
        ])
        table.float_format = '.4'
        total_mem_insts = self.calculate_total_mem_accesses()
        out_of_loop = self.calculate_out_of_loop() / total_mem_insts
        row = [out_of_loop]
        for level in xrange(1):
            result = (
                self.calculate_stream_breakdown(level)
            )
            print(sum(result.values()))
            print(total_mem_insts)
            if level == 0:
                row += [
                    result['RECURSIVE'] / total_mem_insts,
                    result['INCONTINUOUS'] / total_mem_insts,
                    result['INDIRECT_CONTINUOUS'] / total_mem_insts,
                ]
            row += [
                result['AFFINE'] / total_mem_insts,
                result['RANDOM'] / total_mem_insts,
                result['AFFINE_BASE'] / total_mem_insts,
                result['RANDOM_BASE'] / total_mem_insts,
                result['POINTER_CHASE'] / total_mem_insts,
                result['CHAIN_BASE'] / total_mem_insts,
                result['MULTI_BASE'] / total_mem_insts,
            ]
        table.add_row(row)
        print(table)

    def _collect_stream_length(self):
        result = list()
        for inst in self.inst_to_loop_level:
            streams = self.inst_to_loop_level[inst]
            chosen_stream = None
            for stream in streams:
                if stream.stream_class == 'AFFINE':
                    chosen_stream = stream
            if chosen_stream is not None:
                result.append(chosen_stream)
        if self.next is not None:
            result += self.next._collect_stream_length()
        return result

    def print_stream_length(self):
        result = self._collect_stream_length()
        table = prettytable.PrettyTable([
            '<10',
            '<50',
            '<100',
            '<1000',
            'inf'
        ])
        table.float_format = '.4'
        summed_accesses = [0, 0, 0, 0, 0]
        for stream in result:
            accesses = stream.accesses
            streams = stream.streams
            if accesses == 0 or streams == 0:
                continue
            avg_len = float(accesses) / float(streams)
            if avg_len < 10.0:
                summed_accesses[0] += accesses
            elif avg_len < 50.0:
                summed_accesses[1] += accesses
            elif avg_len < 100.0:
                summed_accesses[2] += accesses
            elif avg_len < 1000.0:
                summed_accesses[3] += accesses
            else:
                summed_accesses[4] += accesses
        print summed_accesses
        total_accesses = sum(summed_accesses)
        table.add_row([x / total_accesses for x in summed_accesses])
        print(table)

    def _collect_stream_addr(self):
        result = list()
        for inst in self.inst_to_loop_level:
            streams = self.inst_to_loop_level[inst]
            chosen_stream = None
            for stream in streams:
                if stream.stream_class == 'AFFINE_BASE':
                    chosen_stream = stream
                    break
            if chosen_stream is not None:
                result.append(chosen_stream)
        if self.next is not None:
            result += self.next._collect_stream_addr()
        return result

    def print_stream_addr(self):
        max_length = 16
        title = [str(i) for i in xrange(1, max_length)]
        title.append('other')
        table = prettytable.PrettyTable(title)
        table.float_format = '.4'
        summed_accesses = [0] * max_length
        result = self._collect_stream_addr()
        for stream in result:
            accesses = stream.accesses
            addr_insts = stream.addr_insts
            if addr_insts <= max_length:
                summed_accesses[addr_insts - 1] += accesses
            else:
                summed_accesses[max_length - 1] += accesses
        print summed_accesses
        total_accesses = sum(summed_accesses)
        for i in xrange(1, max_length):
            summed_accesses[i] += summed_accesses[i - 1]
        if total_accesses != 0:
            table.add_row([x / total_accesses for x in summed_accesses])
        else:
            table.add_row(summed_accesses)
        print(table)

    def _collect_stream_alias(self):
        result = list()
        ignore_stream_classes = {
            'RECURSIVE',
            'INCONTINUOUS',
            'INDIRECT_CONTINUOUS'
        }
        for inst in self.inst_to_loop_level:
            streams = self.inst_to_loop_level[inst]
            if streams[0].stream_class in ignore_stream_classes:
                continue
            result.append(streams[0])
        if self.next is not None:
            result += self.next._collect_stream_alias()
        return result

    def print_stream_alias(self):
        result = self._collect_stream_alias()
        table = prettytable.PrettyTable([
            'Aliased',
            'NotAliased'
        ])
        table.float_format = '.4'
        summed_accesses = [0, 0]
        for stream in result:
            accesses = stream.accesses
            alias_insts = stream.alias_insts
            if alias_insts > 0:
                summed_accesses[0] += accesses
            else:
                summed_accesses[1] += accesses
        print summed_accesses
        total_accesses = self.calculate_total_mem_accesses()
        print sum(summed_accesses) / total_accesses
        table.add_row([x / total_accesses for x in summed_accesses])
        print(table)

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
        total_mem_insts = self.stats['DynMemInstCount']
        const_stream = (self.stats['ConstantCount'] /
                        total_mem_insts)

        linear_stream = (self.stats['LinearCount'] /
                         total_mem_insts)

        quardric_stream = (self.stats['QuardricCount'] /
                           total_mem_insts)

        chosen_indirect_stream = (self.stats['IndirectCount'] /
                                  total_mem_insts)

        add_rec_stream = ((self.stats['AddRecLoadCount'] + self.stats['AddRecStoreCount']) /
                          total_mem_insts)

        removed_addr_insts = (self.stats['RemovedAddrInstCount'] /
                              self.stats['TracedDynInstCount'])

        out_of_region = self.calculate_out_of_region() / total_mem_insts

        indirect_accesses, indirect_baseloads, indirect_addr_insts = self.calculate_indirect()

        footprint, reuse = self.calculate_footprint()

        table.add_row([
            const_stream,
            linear_stream,
            quardric_stream,
            indirect_accesses / total_mem_insts,
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

    # def dump_csv(self, fn):
    #     vals = list(self.accesses.values())
    #     Access.dump_csv(vals, fn)
