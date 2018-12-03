
import StreamStatistics
import Util

import pickle

import numpy as np
import matplotlib.pyplot as plt

stream_stats_pickled = [
    'Plots/sdvbs.stream_stats.dat'
]

stream_stats = list()
for p in stream_stats_pickled:
    with open(p, mode='rb') as f:
        stream_stats.append(pickle.load(f))

benchmarks = list()

out_of_loops = list()
incontinuous = list()
unqualified = list()
affine = list()
indirect = list()
pointer_chase = list()

for ss in stream_stats:
    table = StreamStatistics.StreamStatistics.print_benchmark_stream_breakdown(
        ss)
    for row in xrange(len(table.headers)):
        '''
        Fields in one row:
        0. out_of_loop 
        1. recursive
        2. incontinuous 
        3. indirect_incontinuous 
        4. unqualified
        5. affine
        6. indirect
        7. pointer_chase
        8. random
        9. multi_iv
        '''
        benchmark = table.headers[row]
        breakdowns = table.datas[row]
        benchmarks.append(benchmark)
        out_of_loops.append(breakdowns[0] + breakdowns[8] + breakdowns[9])
        incontinuous.append(breakdowns[1] + breakdowns[2] + breakdowns[3])
        unqualified.append(breakdowns[4])
        affine.append(breakdowns[5])
        indirect.append(breakdowns[6])
        pointer_chase.append(breakdowns[7])

N = len(benchmarks)
ind = np.arange(N)    # the x locations for the groups
width = 0.35       # the width of the bars: can also be len(x) sequence

p1 = plt.bar(ind, out_of_loops, width)
p2 = plt.bar(ind, incontinuous, width, bottom=out_of_loops)
p3 = plt.bar(ind, unqualified, width, bottom=incontinuous)
p4 = plt.bar(ind, affine, width, bottom=unqualified)
p5 = plt.bar(ind, indirect, width, bottom=affine)
p6 = plt.bar(ind, pointer_chase, width, bottom=indirect)

plt.ylabel('Scores')
plt.title('Scores by group and gender')
plt.xticks(ind, benchmarks)
plt.yticks(np.arange(0, 1, 0.1))
plt.legend([p1[0], p2[0], p3[0], p4[0], p5[0], p6[0]],
           ['OutOfLoop',
            'NotFullyInlined', 'Unqualified', 'Affine', 'Indirect', 'PointerChase'])

fn = 'Plots/stream-breakdown.pdf'
plt.savefig(fn)
Util.call_helper([
    'pdfcrop',
    fn,
    fn
])
