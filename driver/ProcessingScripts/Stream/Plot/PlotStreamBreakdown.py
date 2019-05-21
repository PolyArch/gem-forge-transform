import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt

import Utils.StreamMessage_pb2 as pb
import Util
import PlotUtil


class PlotStreamBreakdown(object):
    def __init__(self, benchmark_stats):
        self.benchmark_stats = benchmark_stats
        self.affine_accesses = np.zeros(len(self.benchmark_stats))
        self.indirect_accesses = np.zeros(len(self.benchmark_stats))
        self.pointer_chase_accesses = np.zeros(len(self.benchmark_stats))
        self.unqualified_accesses = np.zeros(len(self.benchmark_stats))
        self.not_inlined_accesses = np.zeros(len(self.benchmark_stats))
        for idx in range(len(self.benchmark_stats)):
            self.process_transform_stats(idx)
        print('Average affine {affine}'.format(
            affine=np.mean(self.affine_accesses)))
        print('Average indirect {indirect}'.format(
            indirect=np.mean(self.indirect_accesses)))
        print('Average pointer cahse {ptr}'.format(
            ptr=np.mean(self.pointer_chase_accesses)))

        # Reorganize them into a large array.
        self.values = [
            self.affine_accesses,
            self.indirect_accesses,
            self.pointer_chase_accesses,
            self.unqualified_accesses,
            self.not_inlined_accesses
        ]
        self.labels = [
            'Affine',
            'Indirect',
            'Pointer Chase',
            'Unqualified',
            'Not Fully Inlined',
        ]
        self.benchmark_names = [PlotUtil.get_benchmark_name(b.benchmark.get_name())
                                for b in self.benchmark_stats]

        self.plot()

    def process_transform_stats(self, benchmark_idx):
        sbs = self.benchmark_stats[benchmark_idx]
        stats = sbs.stream_transform_stats

        total_mem_accesses = stats.get_total_mem_accesses()
        affine_accesses = \
            stats.get_total_stream_accesses_by_type(
                pb.StreamValuePattern.Value('CONSTANT')) + \
            stats.get_total_stream_accesses_by_type(
                pb.StreamValuePattern.Value('LINEAR')) + \
            stats.get_total_stream_accesses_by_type(
                pb.StreamValuePattern.Value('QUARDRIC'))
        self.affine_accesses[benchmark_idx] = \
            affine_accesses / total_mem_accesses

        indirect_accesses = \
            stats.get_total_stream_accesses_by_type(
                pb.StreamValuePattern.Value('INDIRECT'))
        self.indirect_accesses[benchmark_idx] = \
            indirect_accesses / total_mem_accesses

        pointer_chase_accesses = \
            stats.get_total_stream_accesses_by_type(
                pb.StreamValuePattern.Value('POINTER_CHASE'))
        self.pointer_chase_accesses[benchmark_idx] = \
            pointer_chase_accesses / total_mem_accesses

        total_inline_loop_mem_accesses = \
            stats.get_total_inline_loop_mem_accesses()
        self.unqualified_accesses[benchmark_idx] = \
            (total_inline_loop_mem_accesses - affine_accesses -
             indirect_accesses - pointer_chase_accesses) / total_mem_accesses

        self.not_inlined_accesses[benchmark_idx] = \
            (total_mem_accesses - total_inline_loop_mem_accesses) / total_mem_accesses

    def plot(self):
        # Compute average.
        self.benchmark_names.append('avg.')
        for i in range(len(self.values)):
            self.values[i] = np.append(self.values[i], np.mean(self.values[i]))

        N = len(self.benchmark_names)
        width = 0.75
        ind = np.arange(N) - width / 2

        mpl.rcParams['hatch.linewidth'] = 0.1

        bar_color_step = 0.8 / (len(self.values) - 3)
        bar_color = np.ones(3) * 0.2
        bottom = [0] * len(self.benchmark_names)
        ps = list()
        for i in xrange(len(self.values)):
            l = self.values[i]
            if i == len(self.values) - 1:
                ps.append(plt.bar(ind, l, width, bottom=bottom, label=self.labels[i],
                                  color='none', edgecolor='k', linewidth=0.1,
                                  hatch='xxxxxx'))
            elif i == len(self.values) - 2:
                ps.append(plt.bar(ind, l, width, bottom=bottom, label=self.labels[i],
                                  color='none', edgecolor='k', linewidth=0.1,
                                  hatch='//////'))
            else:
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

        fn = 'stream-breakdown.pdf'
        plt.savefig(fn)
        Util.call_helper([
            'pdfcrop',
            fn,
            fn
        ])
        plt.clf()
