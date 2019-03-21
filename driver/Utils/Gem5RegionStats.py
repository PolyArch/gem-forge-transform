
import Gem5Stats


class Gem5RegionStats:
    def __init__(self, benchmark, fn):
        self.benchmark = benchmark
        self.fn = fn
        self.regions = dict()
        self.region_parent = dict()
        tail = None
        if isinstance(fn, list):
            # Multiple region stat files to merge.
            assert(fn)
            self.parse(fn[0])
            if len(fn) > 1:
                tail = Gem5RegionStats(benchmark, fn[1:])
        else:
            # This is a single file to process.
            self.parse(fn)
        if tail is not None:
            # Merge the other stats.
            self.merge(tail)

    def __getitem__(self, region):
        return self.regions[region]

    def get_default(self, region, key, default):
        if region in self.regions:
            if key in self.regions[region]:
                return self.regions[region][key]
        return default

    def parse(self, fn):
        try:
            stats = open(fn, 'r')
        except IOError:
            print('Failed to open region stats {fn}.'.format(fn=fn))
        else:
            with stats:
                region_id = None
                stat_lines = list()
                for line in stats:
                    fields = line.split()
                    if fields[0] == '----':
                        # Handle the previous region.
                        if region_id is not None:
                            assert(len(stat_lines) > 0)
                            self.regions[region_id] = Gem5Stats.Gem5Stats(
                                self.benchmark, self.fn, stat_lines)
                            stat_lines = list()
                        region_id = fields[1]
                    elif fields[0] == '-parent':
                        if len(fields) == 2:
                            # This region has a parent.
                            self.region_parent[region_id] = fields[1]
                        else:
                            # This region has no parent.
                            pass
                    else:
                        stat_lines.append(line)
                # Remember to process the last region.
                if region_id is not None:
                    assert(len(stat_lines) > 0)
                    self.regions[region_id] = Gem5Stats.Gem5Stats(
                        self.benchmark, self.fn, stat_lines)

    def merge(self, other):
        assert(self.benchmark == other.benchmark)
        # Merge region parent.
        for region in other.region_parent:
            parent = other.region_parent[region]
            if region in self.region_parent:
                assert(self.region_parent[region] == parent)
            else:
                self.region_parent[region] = parent
        # Merge region stats.
        for region in other.regions:
            stats = other.regions[region]
            if region in self.regions:
                # Add the stats together.
                self.regions[region].merge(stats)
            else:
                # Copy other's stats.
                self.regions[region] = stats

    def print_regions(self):
        children = dict()
        roots = list()
        for region in self.regions:
            if region in self.region_parent:
                parent = self.region_parent[region]
                if parent not in children:
                    children[parent] = list()
                children[parent].append(region)
            else:
                # This region has no parent.
                roots.append(region)
        stack = list()
        for root in roots:
            stack.append((root, 0))
        print('================= Regions Tree ====================')
        while stack:
            region, level = stack.pop()
            print(('  ' * level) + region)
            if region in children:
                for child in children[region]:
                    stack.append((child, level + 1))
        print('================= Regions Tree ====================')
