"""
Simple process script to process stats for work items cycles break down.
Assumes future_cpus00 is in charge of handling work_items.
"""

import re


class TileStats(object):
    def __init__(self, tile_id):
        self.tile_id = tile_id
        self.work_item_ticks = list()
        pass


class TileStatsParser(object):
    def __init__(self, tile_stats):
        self.tile_stats = tile_stats
        self.re = {
            'work_item_ticks': re.compile(self.format_re(
                'system\.work_item_type[0-9]+\.sum')),
        }

    def format_re(self, expression):
        return expression.format(
            tile_id='[0]*{x}'.format(x=self.tile_stats.tile_id))

    def parse(self, fields):
        if len(fields) < 2:
            return
        for k in self.re:
            if not self.re[k].match(fields[0]):
                continue
            self.tile_stats.__dict__[k].append(float(fields[1]))


def process(f):
    tile_stats = [TileStats(i) for i in range(16)]
    tile_stats_parser = [TileStatsParser(ts) for ts in tile_stats]
    for line in f:
        if line.find('End Simulation Statistics') != -1:
            break
        fields = line.split()
        for p in tile_stats_parser:
            p.parse(fields)

    def sum_or_nan(vs):
        s = sum(vs)
        if s == 0:
            return float('NaN')
        return s

    def value_or_nan(x, v):
        return x.__dict__[v] if hasattr(x, v) else float('NaN')

    main_ts = tile_stats[0]
    total_work_item_ticks = sum_or_nan(main_ts.work_item_ticks)
    for work_item in range(len(main_ts.work_item_ticks)):
        tick = main_ts.work_item_ticks[work_item]
        print(f'work item {work_item} {tick / total_work_item_ticks} {tick / 500}')


if __name__ == '__main__':
    import sys
    with open(sys.argv[1]) as f:
        process(f)
