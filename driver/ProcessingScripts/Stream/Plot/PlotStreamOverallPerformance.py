import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt

import Utils.StreamMessage_pb2 as pb
import Util
import PlotUtil


class PlotStreamOverallPerformance(object):
    def __init__(self, benchmark_stats):
        self.benchmark_stats = benchmark_stats
        self.baseline_cycles = np.zeros(len(self.benchmark_stats))
        self.stride_cycles = np.zeros(len(self.benchmark_stats))
        self.imp_cycles = np.zeros(len(self.benchmark_stats))
        self.isb_cycles = np.zeros(len(self.benchmark_stats))
        self.ideal_prefetch_cycles = np.zeros(len(self.benchmark_stats))
        self.stream_prefetch_cycles = np.zeros(len(self.benchmark_stats))
        self.stream_throttle_cycles = np.zeros(len(self.benchmark_stats))
        self.stream_cache_cycles = np.zeros(len(self.benchmark_stats))

        for idx in range(len(self.benchmark_stats)):
            self.process_transform_stats(idx)

        self.compute_speedup()

        # print('Average affine {affine}'.format(
        #     affine=np.mean(self.affine_accesses)))
        # print('Average indirect {indirect}'.format(
        #     indirect=np.mean(self.indirect_accesses)))
        # print('Average pointer cahse {ptr}'.format(
        #     ptr=np.mean(self.pointer_chase_accesses)))

        self.plot()

    def process_transform_stats(self, benchmark_idx):
        sbs = self.benchmark_stats[benchmark_idx]

        for sbss in sbs.sim_stats:
            transform_id = sbss.transform_config.get_transform_id()
            simulation_id = sbss.simulation_config.get_simulation_id()
            if transform_id == 'replay':
                # All baselines.
                if simulation_id == 'o8.ch2.l3':
                    # baseline
                    self.baseline_cycles[benchmark_idx] = \
                        sbss.get_time()
                elif simulation_id == 'o8.ch2.l3.pfa':
                    self.stride_cycles[benchmark_idx] = \
                        sbss.get_time()
                elif simulation_id == 'o8.ch2.l3.impa':
                    self.imp_cycles[benchmark_idx] = \
                        sbss.get_time()
                elif simulation_id == 'o8.ch2.l3.isba':
                    self.isb_cycles[benchmark_idx] = \
                        sbss.get_time()
                elif simulation_id == 'o8.ch2.l3.ipf1000':
                    self.ideal_prefetch_cycles[benchmark_idx] = \
                        sbss.get_time()
            if transform_id == 'stream.alias':
                if simulation_id == 'stream.o8.ch2.l3.lsq.coalesce.fifo240.gb':
                    self.stream_throttle_cycles[benchmark_idx] = \
                        sbss.get_time()
                elif simulation_id == 'stream.o8.ch2.l3.lsq.coalesce.fifo240.gb.cache':
                    self.stream_cache_cycles[benchmark_idx] = \
                        sbss.get_time()
            if transform_id == 'stream.prefetch.alias':
                if simulation_id == 'stream.o8.ch2.l3.lsq.coalesce.fifo240.gb':
                    self.stream_prefetch_cycles[benchmark_idx] = \
                        sbss.get_time()

    def compute_speedup(self):
        # Compute speedups.
        self.stride_spd = np.divide(self.baseline_cycles, self.stride_cycles)
        self.imp_spd = np.divide(self.baseline_cycles, self.imp_cycles)
        self.isb_spd = np.divide(self.baseline_cycles, self.isb_cycles)
        self.ideal_prefetch_spd = np.divide(
            self.baseline_cycles, self.ideal_prefetch_cycles)
        self.stream_prefetch_spd = np.divide(
            self.baseline_cycles, self.stream_prefetch_cycles)
        self.stream_throttle_spd = np.divide(
            self.baseline_cycles, self.stream_throttle_cycles)
        self.stream_cache_spd = np.divide(
            self.baseline_cycles, self.stream_cache_cycles)

        print('Geomean StreamCache/Stride {spd}'.format(
            spd=PlotUtil.geomean(np.divide(self.stream_cache_spd, self.stride_spd))))
        print('Geomean StreamThrottle/Stride {spd}'.format(
            spd=PlotUtil.geomean(np.divide(self.stream_throttle_spd, self.stride_spd))))
        print('Geomean StreamPrefetch/Stride {spd}'.format(
            spd=PlotUtil.geomean(np.divide(self.stream_prefetch_spd, self.stride_spd))))
        print('Geomean IdealPrefetch/Stride {spd}'.format(
            spd=PlotUtil.geomean(np.divide(self.ideal_prefetch_spd, self.stride_spd))))

        self.values = [
            self.stride_spd,
            # self.imp_spd,
            self.isb_spd,
            self.ideal_prefetch_spd,
            self.stream_prefetch_spd,
            self.stream_throttle_spd,
            self.stream_cache_spd,
        ]

        for i in range(len(self.values)):
            self.values[i] = np.append(
                self.values[i], PlotUtil.geomean(self.values[i]))

        self.labels = [
            'StridePf',
            # 'IMPf',
            'ISBPf',
            'IdealHT',
            'StreamPf',
            'BindingStreamPf',
            'StreamAwareCache',
        ]

        self.num_baselines = len(self.labels) - 3

        self.benchmark_names = [PlotUtil.get_benchmark_name(b.benchmark.get_name())
                                for b in self.benchmark_stats]
        self.benchmark_names.append('geomean.')

    def plot(self):

        N = len(self.benchmark_names)
        num_bars_per_benchmark = self.num_baselines + 1
        width = 1
        width_per_benchmark = num_bars_per_benchmark * width + 1
        ind = np.arange(0, N * width_per_benchmark,
                        width_per_benchmark)

        mpl.rcParams['hatch.linewidth'] = 0.1

        bar_color_step = 0.8 / (self.num_baselines)
        bar_color = np.ones(3) * 0.2
        bottom = [0] * len(self.benchmark_names)
        ps = list()
        for i in xrange(len(self.values)):
            l = self.values[i]
            if i < self.num_baselines:
                # Baselines:
                bar_ind = ind + i * width
                ps.append(plt.bar(bar_ind, l, width, bottom=bottom, label=self.labels[i],
                                  color=bar_color, edgecolor='k', linewidth=0.1,
                                  zorder=3))
                bar_color += np.ones(3) * bar_color_step
            if i == self.num_baselines:
                # StreamPrefetch
                bar_ind = ind + self.num_baselines * width
                ps.append(plt.bar(bar_ind, l, width, bottom=bottom, label=self.labels[i],
                                  color=bar_color, edgecolor='k', linewidth=0.1,
                                  zorder=3))
            if i == self.num_baselines + 1:
                # StreamThrottle
                bar_ind = ind + self.num_baselines * width
                prev_value = self.values[i - 1]
                diff_value = np.abs(l - prev_value)
                ps.append(plt.bar(bar_ind, diff_value, width, bottom=prev_value, label=self.labels[i],
                                  color=bar_color, edgecolor='k', linewidth=0.1,
                                  hatch='//////',
                                  zorder=3))
            if i == self.num_baselines + 2:
                # StreamThrottle
                bar_ind = ind + self.num_baselines * width
                prev_value = self.values[i - 1]
                diff_value = np.abs(l - prev_value)
                ps.append(plt.bar(bar_ind, diff_value, width, bottom=prev_value, label=self.labels[i],
                                  color=bar_color, edgecolor='k', linewidth=0.1,
                                  hatch='xxxxxx',
                                  zorder=3))

        font = mpl.font_manager.FontProperties()
        font.set_size(6)
        font.set_weight('bold')

        plt.grid(zorder=0, axis='y')
        plt.xticks(ind + num_bars_per_benchmark * width / 2, self.benchmark_names, horizontalalignment='right',
                   rotation=80, fontproperties=font)
        plt.yticks(np.arange(0, 7.0, 0.5))
        plt.xlim(left=ind[0]-width, right=ind[N-1]+width_per_benchmark-width)
        # plt.legend(handles=ps[::-1], loc=2, bbox_to_anchor=(1, 1))
        plt.legend(handles=ps[::-1], loc=0,
                   prop={'size': 6})

        plt.subplots_adjust(top=0.85, bottom=0.45,
                            left=0.2, right=0.73, wspace=0.01)

        fn = 'stream-overall-performance.pdf'
        plt.savefig(fn)
        Util.call_helper([
            'pdfcrop',
            fn,
            fn
        ])
        plt.clf()
