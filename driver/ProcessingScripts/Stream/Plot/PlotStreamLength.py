import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt

import Utils.StreamMessage_pb2 as pb
import Util
import PlotUtil


class PlotStreamLength(object):
    def __init__(self, benchmark_stats):
        self.benchmark_stats = benchmark_stats
        self.values = [
            np.zeros(len(self.benchmark_stats)),
            np.zeros(len(self.benchmark_stats)),
            np.zeros(len(self.benchmark_stats)),
            np.zeros(len(self.benchmark_stats)),
        ]
        self.thresholds = [
            1000, 100, 50, 0
        ]
        for idx in range(len(self.benchmark_stats)):
            self.process_transform_stats(idx)
        for idx in range(len(self.thresholds)):
            print('Average length > {threshold} is {avg}.'.format(
                threshold=self.thresholds[idx],
                avg=np.mean(self.values[idx])
            ))

        self.labels = [
            '>1k',
            '>100',
            '>50',
            '>0',
        ]
        self.benchmark_names = [PlotUtil.get_benchmark_name(b.benchmark.get_name())
                                for b in self.benchmark_stats]

        self.plot()

    def process_transform_stats(self, benchmark_idx):
        sbs = self.benchmark_stats[benchmark_idx]
        stats = sbs.stream_transform_stats

        total_stream_accesses = stats.get_stream_accesses_longer_than(0)
        for idx in range(len(self.thresholds)):
            accesses = stats.get_stream_accesses_longer_than(
                self.thresholds[idx])
            self.values[idx][benchmark_idx] = accesses / total_stream_accesses
        # Get the difference.
        for idx in range(len(self.thresholds) - 1, 0, -1):
            self.values[idx][benchmark_idx] -= \
                self.values[idx - 1][benchmark_idx]

    def plot(self):
        # Compute average.
        self.benchmark_names.append('avg.')
        for i in range(len(self.values)):
            self.values[i] = np.append(self.values[i], np.mean(self.values[i]))

        N = len(self.benchmark_names)
        width = 0.75
        ind = np.arange(N) - width / 2

        mpl.rcParams['hatch.linewidth'] = 0.1

        bar_color_step = 0.8 / (len(self.values) - 1)
        bar_color = np.ones(3) * 0.2
        bottom = [0] * len(self.benchmark_names)
        ps = list()
        for i in xrange(len(self.values)):
            l = self.values[i]
            ps.append(plt.bar(ind, l, width, bottom=bottom, label=self.labels[i],
                              color=bar_color, edgecolor='k', linewidth=0.1))
            bottom = bottom + l
            bar_color += np.ones(3) * bar_color_step

        font = mpl.font_manager.FontProperties()
        font.set_size(6)
        font.set_weight('bold')

        plt.xticks(ind, self.benchmark_names, horizontalalignment='right',
                   rotation=80, fontproperties=font)
        plt.yticks(np.arange(0, 1.001, 0.1))
        plt.legend(handles=ps[::-1], loc=2, bbox_to_anchor=(1, 1))

        plt.subplots_adjust(top=0.85, bottom=0.45,
                            left=0.2, right=0.73, wspace=0.01)

        fn = 'stream-length.pdf'
        plt.savefig(fn)
        Util.call_helper([
            'pdfcrop',
            fn,
            fn
        ])
        plt.clf()
