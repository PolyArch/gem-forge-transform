import prettytable
import Util

from Utils import SimpleTable

import os


def max_two_array(a, b):
    assert(len(a) == len(b))
    return [max(a[i], b[i]) for i in xrange(len(a))]


def add_two_array(a, b):
    assert(len(a) == len(b))
    return [a[i] + b[i] for i in xrange(len(a))]


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
        self.qualified = fields[14] == 'YES'
        self.chosen = fields[15] == 'YES'
        self.loop_paths = int(fields[16])

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
            'Qualified',
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
            self.qualified,
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


class StreamLoopInfoConfiguredStream:
    def __init__(self, line):
        assert(line[0] == '(')
        fields = line.split(' ')
        self.type = fields[0][1:]
        self.func = fields[1]
        self.inst = self.func + '::' + fields[3][0:-1]
        self.coalesced_id = int(fields[4])

    def isIV(self):
        return self.type == 'IV'

    def isLoad(self):
        return self.type == 'MEM' and 'load' in self.inst

    def isStore(self):
        return self.type == 'MEM' and 'store' in self.inst


class StreamLoopInfo:
    def __init__(self, fn):
        self.name = None
        self.parent_name = None
        self.parent = None
        self.nested_loops = list()
        self.configured_streams = list()
        with open(fn) as f:
            cur_line = 0
            for line in f:
                if cur_line == 0:
                    self.parse_header_line(line)
                if line[0] == '(':
                    self.configured_streams.append(
                        StreamLoopInfoConfiguredStream(line))
                cur_line += 1
        assert(self.name is not None)
        assert(self.parent_name is not None)

        self.ivs = 0
        self.loads = 0
        self.stores = 0
        self.coalesced_loads = 0
        self.coalesced_stores = 0
        seen_coalesced_id = set()
        for s in self.configured_streams:
            if s.isIV():
                self.ivs += 1
            elif s.isLoad():
                self.loads += 1
                if s.coalesced_id != -1:
                    if s.coalesced_id not in seen_coalesced_id:
                        self.coalesced_loads += 1
                        seen_coalesced_id.add(s.coalesced_id)
            elif s.isStore():
                self.stores += 1
                if s.coalesced_id != -1:
                    if s.coalesced_id not in seen_coalesced_id:
                        self.coalesced_stores += 1
                        seen_coalesced_id.add(s.coalesced_id)

    def parse_header_line(self, line):
        fields = line.split(' ')
        self.name = fields[0]
        self.parent_name = fields[1]

    def add_nested_loop(self, loop_info):
        self.nested_loops.append(loop_info)
        assert(loop_info.parent is None)
        loop_info.parent = self

    def get_static_max_n_alive_streams(self):
        n_streams = [
            self.ivs + self.loads + self.stores,
            self.ivs + self.coalesced_loads + self.coalesced_stores,
            self.ivs,
            self.loads,
            self.stores,
            self.coalesced_loads,
            self.coalesced_stores,
        ]
        nested_streams = [0] * len(n_streams)
        for nli in self.nested_loops:
            nested_result = nli.get_static_max_n_alive_streams()
            nested_streams = max_two_array(nested_streams, nested_result)
        return add_two_array(n_streams, nested_streams)


class StreamStatistics:
    def __init__(self, benchmark, stream_tdgs):
        self.next = None
        self.benchmark = benchmark

        self.loop_info = dict()
        self.inst_to_loop_level = dict()

        # Parse the stat file.
        self.parse(stream_tdgs[0] + '.stats.txt')

        # Parse loop info in the extra folder.
        self.parse_extra_folder(stream_tdgs[0] + '.extra')

        # Recursively parse any other files.
        if len(stream_tdgs) > 1:
            self.next = StreamStatistics(benchmark, stream_tdgs[1:])

        self.group_by_loop_level()

    def group_by_loop_level(self):
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

    def parse_extra_folder(self, extra_folder):
        for filename in os.listdir(extra_folder):
            if filename.endswith('.info.txt') and (not filename.startswith('(')):
                # This is a loop info file.
                self.parse_loop_info_txt(os.path.join(extra_folder, filename))
        # Build the tree between parent loop and children loops.
        for name in self.loop_info:
            li = self.loop_info[name]
            if li.parent_name in self.loop_info:
                parent_li = self.loop_info[li.parent_name]
                parent_l1.add_nested_loop(li)

    def parse_loop_info_txt(self, fn):
        loop_info = StreamLoopInfo(fn)
        self.loop_info[loop_info.name] = loop_info

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

    def _collect_stream_breakdown(self, level):
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
            'UNQUALIFIED': 0,
            'AFFINE': 0,
            'INDIRECT': 0,
            'POINTER_CHASE': 0,
            'RANDOM': 0,
            'MULTI_IV': 0,
        }
        for access in filtered:
            good_classes = {'AFFINE', 'INDIRECT',
                            'POINTER_CHASE', 'RANDOM', 'MULTI_IV'}
            if access.stream_class in good_classes:
                if access.qualified:
                    result[access.stream_class] += access.accesses
                else:
                    result['UNQUALIFIED'] += access.accesses
            else:
                result[access.stream_class] += access.accesses
        if self.next is not None:
            next_result = self.next._collect_stream_breakdown(level)
            for field in result:
                result[field] += next_result[field]
        return result

    @staticmethod
    def get_stream_breakdown_title():
        return [
            'OutOfLoop',
            'Recursive',
            'Incontinuous',
            'IndirectContinuous',
            'UNQUALIFIED',
            'AFFINE',
            'INDIRECT',
            'POINTER_CHASE',
            'RAMDOM',
            'MULTI_IV',
        ]

    def get_stream_breakdown_row(self, level):
        total_mem_insts = self.calculate_total_mem_accesses()
        out_of_loop = self.calculate_out_of_loop() / total_mem_insts
        row = [out_of_loop]
        result = (
            self._collect_stream_breakdown(level)
        )
        if level == 0:
            row += [
                result['RECURSIVE'] / total_mem_insts,
                result['INCONTINUOUS'] / total_mem_insts,
                result['INDIRECT_CONTINUOUS'] / total_mem_insts,
                result['UNQUALIFIED'] / total_mem_insts,
            ]
        row += [
            result['AFFINE'] / total_mem_insts,
            result['INDIRECT'] / total_mem_insts,
            result['POINTER_CHASE'] / total_mem_insts,
            result['RANDOM'] / total_mem_insts,
            result['MULTI_IV'] / total_mem_insts,
        ]
        return row

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

    def _collect_stream_qualified(self, level):
        result = 0
        for inst in self.inst_to_loop_level:
            streams = self.inst_to_loop_level[inst]
            if len(streams) <= level:
                continue
            stream = streams[level]
            if stream.qualified:
                result += stream.accesses
        if self.next is not None:
            result += self.next._collect_stream_qualified(level)
        return result

    def print_stream_qualified(self):
        title = list()
        for i in xrange(3):
            title.append('L{i}'.format(i=i))
        row = [self._collect_stream_qualified(level) for level in xrange(3)]
        table = prettytable.PrettyTable(title)
        table.float_format = '.4'
        total_accesses = self.calculate_total_mem_accesses()
        table.add_row([x / float(total_accesses) for x in row])
        print('total_access is ' + str(total_accesses))
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

    def _collect_chosen_level(self):
        max_level = 0
        result = [0] * max_level
        for inst in self.inst_to_loop_level:
            streams = self.inst_to_loop_level[inst]
            for i in xrange(min(max_level, len(streams))):
                stream = streams[i]
                if stream.chosen:
                    result[i] += stream.accesses
                    break
        if self.next is not None:
            next_result = self.next._collect_chosen_level()
            for i in xrange(max_level):
                result += next_result[i]
        return result

    def print_chosen_level(self):
        result = self._collect_chosen_level()
        title = list()
        for i in xrange(len(result)):
            title.append('L{i}'.format(i=i))
        total_accesses = self.calculate_total_mem_accesses()
        table = prettytable.PrettyTable(title)
        table.add_row([float(x) / total_accesses for x in result])

    def _collect_chosen_stream(self):
        result = list()
        for inst in self.inst_to_loop_level:
            streams = self.inst_to_loop_level[inst]
            for i in xrange(len(streams)):
                stream = streams[i]
                if stream.chosen:
                    result.append(stream)
                    break
        if self.next is not None:
            result += self.next._collect_chosen_stream()
        return result

    def print_access(self):
        vals = list(self.accesses.values())
        Access.print_table(vals)

    def _collect_stream_loop_paths(self, max_paths):
        result = [0] * max_paths
        for inst in self.inst_to_loop_level:
            streams = self.inst_to_loop_level[inst]
            # Only check the inner most loop level.
            stream = streams[0]
            if stream.loop_paths >= max_paths:
                result[max_paths - 1] += stream.accesses
            else:
                result[stream.loop_paths - 1] += stream.accesses
        if self.next is not None:
            next_result = self.next._collect_stream_loop_paths(max_paths)
            for i in xrange(len(result)):
                result[i] += next_result[i]
        return result

    def normalize_with_total_accesses(self, row):
        total = self.calculate_total_mem_accesses()
        return [x / total for x in row]

    def collect_stats(self):
        result = dict()
        for key in self.stats:
            result[key] = self.stats[key]
        if self.next is not None:
            next_result = self.next.collect_stats()
            for key in next_result:
                result[key] += next_result[key]
        return result

    def print_stats(self):
        result = self.collect_stats()
        title = list()
        data = list()
        for key in result:
            title.append(key)
            data.append(result[key])
        title.append('Removed(%)')
        data.append(result['DeletedInstCount']/result['DynInstCount'] * 100)
        title.append('Added(%)')
        data.append((result['ConfigInstCount'] +
                     result['StepInstCount'])/result['DynInstCount'] * 100)
        title.append('ConfigPMI')
        data.append(result['ConfigInstCount']/result['DynInstCount'] * 1000000)
        table = prettytable.PrettyTable(title)
        table.float_format = '.2'
        table.add_row(data)
        print(table)

    # def dump_csv(self, fn):
    #     vals = list(self.accesses.values())
    #     Access.dump_csv(vals, fn)

    @staticmethod
    def normalize_row(row):
        total = sum(row)
        if total > 0.0:
            new_row = [x / total for x in row]
            return new_row
        else:
            return row

    @staticmethod
    def print_benchmark_stream_breakdown(benchmark_statistic_map):
        title = StreamStatistics.get_stream_breakdown_title()
        table = SimpleTable.SimpleTable('Benchmark', title)
        for benchmark in benchmark_statistic_map:
            stats = benchmark_statistic_map[benchmark]
            row = stats.get_stream_breakdown_row(0)
            table.add_row(benchmark, row)
        print(table)
        return table

    @staticmethod
    def print_benchmark_stream_breakdown_coarse(benchmark_statistic_map):
        title = [
            'OutOfLoop',
            'Incontinuous',
            'Stream',
        ]
        title.insert(0, 'Benchmark')
        table = prettytable.PrettyTable(title)
        for benchmark in benchmark_statistic_map:
            stats = benchmark_statistic_map[benchmark]
            row = stats.get_stream_breakdown_row(0)
            coarse_row = [
                row[0],
                row[2],
                sum(row[4:-1])
            ]
            coarse_row = StreamStatistics.normalize_row(coarse_row)
            coarse_row.insert(0, benchmark)
            table.add_row(coarse_row)
        table.float_format = '.4'
        print(table)

    @staticmethod
    def print_benchmark_stream_breakdown_indirect(benchmark_statistic_map):
        title = [
            'Affine',
            'Indirect',
        ]
        title.insert(0, 'Benchmark')
        table = prettytable.PrettyTable(title)
        for benchmark in benchmark_statistic_map:
            stats = benchmark_statistic_map[benchmark]
            row = stats.get_stream_breakdown_row(0)
            indirect_row = [
                row[4] + row[6],
                row[5] + sum(row[7:-1])
            ]
            indirect_row = StreamStatistics.normalize_row(indirect_row)
            indirect_row.insert(0, benchmark)
            table.add_row(indirect_row)
        table.float_format = '.4'
        print(table)

    @staticmethod
    def print_benchmark_stream_paths(benchmark_statistic_map):
        title = [
            'Benchmark'
        ]
        max_paths = 5
        for i in xrange(1, max_paths):
            title.append('{i}'.format(i=i))
        title.append('>={max_paths}'.format(max_paths=max_paths))
        table = prettytable.PrettyTable(title)
        for benchmark in benchmark_statistic_map:
            stats = benchmark_statistic_map[benchmark]
            row = stats._collect_stream_loop_paths(max_paths)
            # Normalize with ourselves.
            row = StreamStatistics.normalize_row(row)
            row.insert(0, benchmark)
            table.add_row(row)
        table.float_format = '.4'
        print(table)

    def get_chosen_stream_length_row(self):
        streams = self._collect_chosen_stream()
        row = [0, 0, 0, 0, 0]
        for stream in streams:
            accesses = stream.accesses
            streams = stream.streams
            if accesses == 0 or streams == 0:
                continue
            avg_len = float(accesses) / float(streams)
            if avg_len < 10.0:
                row[0] += accesses
            elif avg_len < 50.0:
                row[1] += accesses
            elif avg_len < 100.0:
                row[2] += accesses
            elif avg_len < 1000.0:
                row[3] += accesses
            else:
                row[4] += accesses
        return row

    @staticmethod
    def print_benchmark_chosen_stream_length(benchmark_statistic_map):
        columns = [
            '<10',
            '<50',
            '<100',
            '<1000',
            'inf'
        ]
        table = SimpleTable.SimpleTable('Benchmark', columns)
        for benchmark in benchmark_statistic_map:
            stats = benchmark_statistic_map[benchmark]
            row = stats.get_chosen_stream_length_row()
            # Normalize with ourselves.
            row = StreamStatistics.normalize_row(row)
            table.add_row(benchmark, row)
        print(table)
        return table

    def get_chosen_stream_alive_streams_dict(self):
        streams = self._collect_chosen_stream()
        result = dict()
        for stream in streams:
            accesses = stream.accesses
            loop = stream.loop
            # Iterate through all traces to find the loop.
            trace = self
            while trace is not None:
                if loop in trace.loop_info:
                    loop_info = trace.loop_info[loop]
                    break
                trace = trace.next
            assert(loop_info is not None)
            alive_streams = loop_info.get_static_max_n_alive_streams()[1]
            if alive_streams not in result:
                result[alive_streams] = 0 
            result[alive_streams] += accesses 
        return result

    @staticmethod
    def print_benchmark_chosen_stream_alive_streams(benchmark_statistic_map):
        columns = [
            '<=8',
            '<=16',
            '<=24',
            '<=32',
            '>32',
        ]
        table = SimpleTable.SimpleTable('Benchmark', columns)
        sep = xrange(0, 33, 8)
        for benchmark in benchmark_statistic_map:
            row = [0] * len(columns)
            stats = benchmark_statistic_map[benchmark]
            result = stats.get_chosen_stream_alive_streams_dict()
            for k in result:
                found = False
                for i in xrange(len(sep) - 1):
                    lhs = sep[i] + 1
                    rhs = sep[i + 1]
                    if k >= lhs and k <= rhs:
                        found = True
                        row[i] += result[k]
                if not found:
                    row[-1] += result[k]
            print row
            row = StreamStatistics.normalize_row(row)
            print row
            table.add_row(benchmark, row)
        print(table)
        return table



    def get_chosen_stream_percentage_row(self):
        streams = self._collect_chosen_stream()
        row = [0]
        for stream in streams:
            accesses = stream.accesses
            row[0] += accesses
        return row

    @staticmethod
    def print_benchmark_chosen_stream_percentage(benchmark_statistic_map):
        title = [
            'Benchmark',
            'Percentage',
        ]
        table = prettytable.PrettyTable(title)
        for benchmark in benchmark_statistic_map:
            stats = benchmark_statistic_map[benchmark]
            row = stats.get_chosen_stream_percentage_row()
            # Normalize with total accesses
            row = stats.normalize_with_total_accesses(row)
            row.insert(0, benchmark)
            table.add_row(row)
        table.float_format = '.4'
        print(table)

    def get_chosen_stream_indirect(self):
        streams = self._collect_chosen_stream()
        row = [0, 0]
        indirect_stream_class = {
            'RANDOM',
            'RANDOM_IV',
            'MULTI_IV',
            'AFFINE_BASE',
            'RANDOM_BASE',
            'POINTER_CHASE',
            'CHAIN_BASE',
            'MULTI_BASE',
        }
        for stream in streams:
            accesses = stream.accesses
            if stream.stream_class in indirect_stream_class:
                row[1] += accesses
            else:
                row[0] += accesses
        return row

    @staticmethod
    def print_benchmark_chosen_stream_indirect(benchmark_statistic_map):
        title = [
            'Benchmark',
            'Affine',
            'Indirect',
        ]
        table = prettytable.PrettyTable(title)
        for benchmark in benchmark_statistic_map:
            stats = benchmark_statistic_map[benchmark]
            row = stats.get_chosen_stream_indirect()
            row = StreamStatistics.normalize_row(row)
            row.insert(0, benchmark)
            table.add_row(row)
        table.float_format = '.4'
        print(table)

    def get_chosen_stream_loop_path(self, max_paths):
        streams = self._collect_chosen_stream()
        row = [0] * max_paths
        for stream in streams:
            accesses = stream.accesses
            if stream.loop_paths > max_paths:
                row[max_paths - 1] += accesses
            else:
                row[stream.loop_paths - 1] += accesses
        return row

    @staticmethod
    def print_benchmark_chosen_stream_loop_path(benchmark_statistic_map):
        title = list()
        max_paths = 5
        for i in xrange(1, max_paths):
            title.append('{i}'.format(i=i))
        title.append('>={max_paths}'.format(max_paths=max_paths))
        table = SimpleTable.SimpleTable('Benchmark', title)
        for benchmark in benchmark_statistic_map:
            stats = benchmark_statistic_map[benchmark]
            row = stats.get_chosen_stream_loop_path(max_paths)
            row = StreamStatistics.normalize_row(row)
            table.add_row(benchmark, row)
        print(table)
        return table

    def get_chosen_stream_configure_level_row(self, max_level):
        streams = self._collect_chosen_stream()
        row = [0] * max_level
        for stream in streams:
            accesses = stream.accesses
            if stream.level >= max_level:
                row[max_level - 1] += accesses
            else:
                row[stream.level] += accesses
        return row

    @staticmethod
    def print_benchmark_chosen_stream_configure_level(benchmark_statistic_map):
        title = [
            'Benchmark'
        ]
        max_level = 3
        for i in xrange(0, max_level - 1):
            title.append('{i}'.format(i=i))
        title.append('>={max_level}'.format(max_level=max_level - 1))
        table = prettytable.PrettyTable(title)
        for benchmark in benchmark_statistic_map:
            stats = benchmark_statistic_map[benchmark]
            row = stats.get_chosen_stream_configure_level_row(max_level)
            row = StreamStatistics.normalize_row(row)
            row.insert(0, benchmark)
            table.add_row(row)
        table.float_format = '.4'
        print(table)

    def get_static_max_n_alive_streams(self):
        n_streams = [0] * 7
        for li_name in self.loop_info:
            li = self.loop_info[li_name]
            if li.parent is not None:
                # This is not the outer most loop.
                continue
            n_streams = max_two_array(
                n_streams, li.get_static_max_n_alive_streams())
        if self.next is not None:
            n_streams = max_two_array(
                n_streams, self.next.get_static_max_n_alive_streams())
        return n_streams

    @staticmethod
    def print_benchmark_static_max_n_alive_streams(benchmark_statistic_map):
        title = [
            'Benchmark',
            'Max Alive Streams',
            'Max Alive Coalesced',
            'IVs',
            'Loads',
            'Stores',
            'Coalesced Loads',
            'Coalesced Stores',
        ]
        table = prettytable.PrettyTable(title)
        for benchmark in benchmark_statistic_map:
            stats = benchmark_statistic_map[benchmark]
            row = stats.get_static_max_n_alive_streams()
            row.insert(0, benchmark)
            table.add_row(row)
        print(table)
