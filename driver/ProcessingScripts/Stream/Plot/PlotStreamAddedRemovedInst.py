import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt

import Utils.StreamMessage_pb2 as pb
import Util
import PlotUtil


class PlotStreamAddedRemovedInst(object):
    def __init__(self, benchmark_stats):
        self.benchmark_stats = benchmark_stats
        self.values = [
            np.zeros(len(self.benchmark_stats)),
            np.zeros(len(self.benchmark_stats)),
        ]

        self.labels = [
            'Remain',
            'Added',
        ]

        for idx in range(len(self.benchmark_stats)):
            self.process_transform_stats(idx)
        for idx in range(len(self.labels)):
            print('Average {label} is {avg}.'.format(
                label=self.labels[idx],
                avg=np.mean(self.values[idx])
            ))

        self.benchmark_names = [PlotUtil.get_benchmark_name(b.benchmark.get_name())
                                for b in self.benchmark_stats]

        self.plot()

    def process_transform_stats(self, benchmark_idx):
        sbs = self.benchmark_stats[benchmark_idx]
        stats = sbs.stream_transform_stats

        total = stats.get_total_insts()
        removed = stats.get_total_removed_insts()
        remain = total - removed
        added = stats.get_total_added_insts()
        self.values[0][benchmark_idx] = float(remain) / float(total)
        self.values[1][benchmark_idx] = float(added) / float(total)

    def plot(self):
        # Compute average.
        self.benchmark_names.append('avg.')
        for i in range(len(self.values)):
            self.values[i] = np.append(self.values[i], np.mean(self.values[i]))

        N = len(self.benchmark_names)
        width = 0.75
        ind = np.arange(N) - width / 2

        mpl.rcParams['hatch.linewidth'] = 0.1

        bar_color_step = 0.8 / (len(self.values))
        bar_color = np.ones(3) * 0.2
        bottom = [0] * len(self.benchmark_names)
        ps = list()
        for i in xrange(len(self.values)):
            l = self.values[i]
            ps.append(plt.bar(ind, l, width, bottom=bottom, label=self.labels[i],
                              color=bar_color, edgecolor='k', linewidth=0.1, zorder=3))
            bottom = bottom + l
            bar_color += np.ones(3) * bar_color_step

        font = mpl.font_manager.FontProperties()
        font.set_size(6)
        font.set_weight('bold')

        plt.grid(zorder=0, axis='y')
        plt.xticks(ind, self.benchmark_names, horizontalalignment='right',
                   rotation=80, fontproperties=font)
        plt.xlim(left=ind[0]-width/2, right=ind[N-1]+width/2)
        plt.yticks(np.arange(0, 1.001, 0.1))
        plt.legend(handles=ps[::-1], loc=2, bbox_to_anchor=(1, 1))

        plt.subplots_adjust(top=0.85, bottom=0.45,
                            left=0.2, right=0.73, wspace=0.01)

        fn = 'stream-added-removed-inst.pdf'
        plt.savefig(fn)
        Util.call_helper([
            'pdfcrop',
            fn,
            fn
        ])
        plt.clf()
