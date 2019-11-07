"""
Simple process script to process stats for multi-core configurations.
Assume MinorCPU.
"""

import re


class TileStats(object):
    def __init__(self, tile_id):
        self.tile_id = tile_id
        self.load_blocked_cpi = 0
        self.load_blocked_ratio = 0
        self.l1d_access = 0
        self.l1d_misses = 0
        self.l2_access = 0
        self.l2_misses = 0
        self.l3_access = 0
        self.l3_misses = 0
        pass


class TileStatsParser(object):
    def __init__(self, tile_stats):
        self.tile_stats = tile_stats
        self.re = {
            'load_blocked_cpi': re.compile(self.format_re(
                'system\.future_cpus{tile_id}\.loadBlockedCPI')),
            'load_blocked_ratio': re.compile(self.format_re(
                'system\.future_cpus{tile_id}\.loadBlockedCyclesPercen*')),
            'l1d_access': re.compile(self.format_re(
                'system\.ruby\.l0_cntrl{tile_id}\.Dcache.demand_accesses')),
            'l1d_misses': re.compile(self.format_re(
                'system\.ruby\.l0_cntrl{tile_id}\.Dcache.demand_misses')),
            'l2_access': re.compile(self.format_re(
                'system\.ruby\.l1_cntrl{tile_id}\.cache.demand_accesses')),
            'l2_misses': re.compile(self.format_re(
                'system\.ruby\.l1_cntrl{tile_id}\.cache.demand_misses')),
            'l3_access': re.compile(self.format_re(
                'system\.ruby\.l2_cntrl{tile_id}\.L2cache.demand_accesses')),
            'l3_misses': re.compile(self.format_re(
                'system\.ruby\.l2_cntrl{tile_id}\.L2cache.demand_misses')),
        }

    def format_re(self, expression):
        return expression.format(tile_id=self.tile_stats.tile_id)

    def parse(self, fields):
        if len(fields) < 2:
            return
        for k in self.re:
            if self.re[k].match(fields[0]):
                self.tile_stats.__dict__[k] = float(fields[1])


def process(f):
    tile_stats = [TileStats(i) for i in range(4)]
    tile_stats_parser = [TileStatsParser(ts) for ts in tile_stats]
    for line in f:
        if line.find('End Simulation Statistics') != -1:
            break
        fields = line.split()
        for p in tile_stats_parser:
            p.parse(fields)

    print('total l3 miss rate {v}'.format(
        v=sum(ts.l3_misses for ts in tile_stats) /
        sum(ts.l3_access for ts in tile_stats)
    ))
    print('total l2 miss rate {v}'.format(
        v=sum(ts.l2_misses for ts in tile_stats) /
        sum(ts.l2_access for ts in tile_stats)
    ))
    print('total l1d miss rate {v}'.format(
        v=sum(ts.l1d_misses for ts in tile_stats) /
        sum(ts.l1d_access for ts in tile_stats)
    ))
    print('load blocked cpi {v}'.format(
        v=sum(ts.load_blocked_cpi for ts in tile_stats) /
        len(tile_stats)
    ))
    print('load blocked percentage {v}'.format(
        v=sum(ts.load_blocked_ratio for ts in tile_stats) /
        len(tile_stats)
    ))


if __name__ == '__main__':
    import sys
    with open(sys.argv[1]) as f:
        process(f)
