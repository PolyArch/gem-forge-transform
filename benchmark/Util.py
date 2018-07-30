
import matplotlib
import matplotlib.pyplot as plt
import numpy

import subprocess


class Gem5Stats:
    def __init__(self, benchmark, fn):
        self.benchmark = benchmark
        self.fn = fn
        self.stats = dict()
        with open(self.fn, 'r') as stats:
            for line in stats:
                if len(line) == 0:
                    continue
                if line[0] == '-':
                    continue
                fields = line.split()
                try:
                    self.stats[fields[0]] = float(fields[1])
                except Exception as e:
                    pass
                    # print('ignore line {line}'.format(line=line))

    def get_default(self, key, default):
        if key in self.stats:
            return self.stats[key]
        else:
            return default

    def __getitem__(self, key):
        return self.stats[key]


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
        with open(fn, 'r') as stats:
            region = None
            region_id = None
            for line in stats:
                fields = line.split()
                if fields[0] == '----':
                    region_id = fields[1]
                    self.regions[region_id] = dict()
                    region = self.regions[region_id]
                elif fields[0] == '-parent':
                    if len(fields) == 2:
                        # This region has a parent.
                        self.region_parent[region_id] = fields[1]
                    else:
                        # This region has no parent.
                        pass
                else:
                    stat_id = fields[0]
                    value = float(fields[1])
                    region[stat_id] = value

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
                my_stats = self.regions[region]
                for stat_id in stats:
                    # assert(stat_id in my_stats)
                    if stat_id in my_stats:
                        my_stats[stat_id] += stats[stat_id]
                    else:
                        my_stats[stat_id] = stats[stat_id]
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


class Variable:
    def __init__(self, name, baseline_key, test_key, color):
        self.name = name
        self.baseline_key = baseline_key
        self.test_key = test_key
        self.color = color


def call_helper(cmd):
    """
    Helper function to call a command and print the actual command when failed.
    """
    print(' '.join(cmd))
    try:
        subprocess.check_call(cmd)
    except subprocess.CalledProcessError as e:
        print('Error when executing {cmd}'.format(cmd=' '.join(cmd)))
        raise e


class Results:
    def __init__(self):
        self.results = dict()

    """
    Add a result.
    
    Parameters
    ----------
    group: which group does this result belongs to, e.g. baseline, replay.
    benchmark: the benchmark name, e.g. fft.strided.
    fn: the name of gem5's output stats file.
    """

    def addResult(self, group, benchmark, fn):
        if group not in self.results:
            self.results[group] = dict()
        self.results[group][benchmark] = Gem5Stats(benchmark, fn)

    """
    Draw the comparison with baseline group and dump to a pdf file.

    Parameters
    ----------
    pdf_fn: the output pdf file name.
    baseline: the baseline group.
    test: the test group.
    variables: a list of Variables.
    """

    def draw(self, pdf_fn, baseline, test, variables):

        assert(baseline in self.results)
        assert(test in self.results)
        assert(len(self.results[baseline]) == len(self.results[test]))

        num_benchmarks = len(self.results[baseline])
        benchmarks = self.results[baseline].keys()
        benchmarks.sort()

        total_spacing = 1.5
        index = numpy.arange(num_benchmarks) * total_spacing
        total_bar_spacing = 0.8 * total_spacing
        bar_width = total_bar_spacing / len(variables)
        pos = 0.0

        ax = plt.subplot(111)
        for v in variables:
            ys = [(self.results[test][b][v.test_key] / self.results[baseline][b][v.baseline_key])
                  for b in benchmarks]
            ax.bar(index + pos, ys, bar_width,
                   color=numpy.array(v.color, ndmin=2) / 256.0, label=v.name)
            pos += bar_width

        box = ax.get_position()
        ax.set_position([box.x0, box.y0 + box.height * 0.2,
                         box.width, box.height * 0.8])
        plt.grid()
        plt.xticks(index + total_bar_spacing / 2.0, benchmarks)
        plt.ylabel('Ratio')

        ax.legend(loc='upper center', bbox_to_anchor=(0.5, 1.05),
                  fancybox=True, shadow=True, ncol=4)
        ax.set_ylim([0.0, 1.2])
        plt.xticks(rotation=90)
        plt.savefig(pdf_fn)
        # plt.show()
        plt.gcf().clear()


class RegionAggStats:

    def __init__(self, prefix, region, name):
        self.prefix = prefix
        self.region = region
        self.name = name
        # Baseline stats.
        self.base_insts = -1.0
        self.base_ratio = -1.0
        self.base_cycles = -1.0
        self.base_issue = -1.0
        self.base_br = -1.0
        self.base_bm = -1.0
        self.base_cm = -1.0

        # ADFA stats.
        self.adfa_speedup = -1.0
        self.adfa_ratio = -1.0
        self.adfa_cfgs = -1.0
        self.adfa_dfs = -1.0
        self.adfa_insts = -1.0
        self.adfa_cycles = -1.0
        self.adfa_df_len = -1.0
        self.adfa_issue = -1.0

        # SIMD stats.
        self.simd_speedup = -1.0
        self.simd_insts = -1.0
        self.simd_cycles = -1.0
        self.simd_issue = -1.0
        self.simd_br = -1.0
        self.simd_bm = -1.0
        self.simd_cm = -1.0

    def compute_baseline(self, baseline):

        n_branch = baseline[self.region][self.prefix + 'fetch.branchInsts']
        n_branch_misses = baseline[self.region][self.prefix +
                                                'fetch.branchPredMisses']
        n_cache_misses = baseline[self.region][self.prefix +
                                               'dcache.demand_misses']

        self.base_insts = baseline[self.region][self.prefix +
                                                'commit.committedInsts']
        base_all_insts = baseline['all'][self.prefix +
                                         'commit.committedInsts']
        self.base_cycles = baseline[self.region][self.prefix + 'numCycles']

        if base_all_insts > 0.0:
            self.base_ratio = self.base_insts / base_all_insts

        if self.base_cycles > 0.0:
            self.base_issue = self.base_insts / self.base_cycles

        if self.base_insts > 0.0:
            self.base_br = n_branch / self.base_insts * 1000.0
            self.base_bm = n_branch_misses / self.base_insts * 1000.0
            self.base_cm = n_cache_misses / self.base_insts * 1000.0

    def compute_adfa(self, adfa):
        self.adfa_cfgs = adfa.get_default(
            self.region, 'tdg.accs.adfa.numConfigured', 0.0)
        self.adfa_dfs = adfa.get_default(
            self.region, 'tdg.accs.adfa.numExecution', 0.0)
        self.adfa_insts = adfa.get_default(
            self.region, 'tdg.accs.adfa.numCommittedInst', 0.0)
        self.adfa_cycles = adfa.get_default(
            self.region, 'tdg.accs.adfa.numCycles', 0.0)

        cpu_cycles = adfa[self.region][self.prefix + 'numCycles']

        if cpu_cycles > 0.0:
            print("Warning cpu cycles is 0 for " + self.region)
            self.adfa_speedup = self.base_cycles / cpu_cycles
            self.adfa_ratio = self.adfa_cycles / cpu_cycles

        if self.adfa_insts > 0.0:
            if self.adfa_dfs > 0.0:
                self.adfa_df_len = self.adfa_insts / self.adfa_dfs
            self.adfa_issue = self.adfa_insts / self.adfa_cycles

    def compute_simd(self, simd):
        n_branch = simd[self.region][self.prefix + 'fetch.branchInsts']
        n_branch_misses = simd[self.region][self.prefix +
                                            'fetch.branchPredMisses']
        n_cache_misses = simd[self.region][self.prefix +
                                           'dcache.demand_misses']

        self.simd_insts = simd[self.region][self.prefix +
                                            'commit.committedInsts']
        self.simd_cycles = simd[self.region][self.prefix + 'numCycles']

        if self.simd_cycles > 0.0:
            self.simd_issue = self.simd_insts / self.simd_cycles
            self.simd_speedup = self.base_cycles / self.simd_cycles

        if self.simd_insts > 0.0:
            self.simd_br = n_branch / self.simd_insts * 1000.0
            self.simd_bm = n_branch_misses / self.simd_insts * 1000.0
            self.simd_cm = n_cache_misses / self.simd_insts * 1000.0

    @staticmethod
    def print_title():
        title = (
            '{name:>50}| '
            '{base_insts:>10} '
            '{base_issue:>10} '
            '{base_ratio:>5} '
            '{bpki:>7} '
            '{bmpki:>7} '
            '{cmpki:>5}| '
            '{adfa_spd:>10} '
            '{adfa_cfgs:>10} '
            '{adfa_dfs:>10} '
            '{adfa_ratio:>10} '
            '{adfa_df_len:>10} '
            '{adfa_issue:>10}| '
            '{simd_spd:>10} '
            '{simd_insts:>10} '
            '{simd_issue:>10} '
            '{simd_bpki:>7} '
            '{simd_bmpki:>7} '
            '{simd_cmpki:>5}| '
        )
        print(title.format(
            name='name',
            base_insts='base_insts',
            base_issue='base_issue',
            base_ratio='ratio',
            bpki='BPKI',
            bmpki='BMPKI',
            cmpki='CMPKI',
            adfa_spd='adfa_spd',
            adfa_cfgs='adfa_cfgs',
            adfa_dfs='adfa_dfs',
            adfa_ratio='adfa_ratio',
            adfa_df_len='adfa_dflen',
            adfa_issue='adfa_issue',
            simd_spd='simd_spd',
            simd_insts='simd_insts',
            simd_issue='simd_issue',
            simd_bpki='BPKI',
            simd_bmpki='BMPKI',
            simd_cmpki='CMPKI',
        ))

    def print_line(self):
        line = (
            '{name:>50}| '
            '{base_insts:>10} '
            '{base_issue:>10.4f} '
            '{base_ratio:>5.2f} '
            '{bpki:>7.1f} '
            '{bmpki:>7.1f} '
            '{cmpki:>5.1f}| '
            '{adfa_spd:>10.2f} '
            '{adfa_cfgs:>10} '
            '{adfa_dfs:>10} '
            '{adfa_ratio:>10.4f} '
            '{adfa_df_len:>10.1f} '
            '{adfa_issue:>10.4f}| '
            '{simd_spd:>10.2f} '
            '{simd_insts:>10} '
            '{simd_issue:>10.4f} '
            '{simd_bpki:>7.1f} '
            '{simd_bmpki:>7.1f} '
            '{simd_cmpki:>5.1f}| '
        )
        print(line.format(
            name=self.name,
            base_insts=self.base_insts,
            base_issue=self.base_issue,
            base_ratio=self.base_ratio*100.0,
            bpki=self.base_br,
            bmpki=self.base_bm,
            cmpki=self.base_cm,
            adfa_spd=self.adfa_speedup,
            adfa_cfgs=self.adfa_cfgs,
            adfa_dfs=self.adfa_dfs,
            adfa_ratio=self.adfa_ratio,
            adfa_df_len=self.adfa_df_len,
            adfa_issue=self.adfa_issue,
            simd_spd=self.simd_speedup,
            simd_insts=self.simd_insts,
            simd_issue=self.simd_issue,
            simd_bpki=self.simd_br,
            simd_bmpki=self.simd_bm,
            simd_cmpki=self.simd_cm,
        ))


class ADFAAnalyzer:

    SYS_CPU_PREFIX = 'system.cpu1.'

    @staticmethod
    def compute_speedup(baseline, adfa, region):
        baseline_cycles = baseline[region][ADFAAnalyzer.SYS_CPU_PREFIX + 'numCycles']
        adfa_cycles = adfa[region][ADFAAnalyzer.SYS_CPU_PREFIX + 'numCycles']
        if adfa_cycles == 0.0:
            print("Warning ADFA cycles is 0 for " + region)
            return 1.0
        return baseline_cycles / adfa_cycles

    @staticmethod
    def compute_runtime_ratio(adfa, region):
        cpu_cycles = adfa[region][ADFAAnalyzer.SYS_CPU_PREFIX + 'numCycles']
        adfa_cycles = ADFAAnalyzer.get_with_default(
            adfa, region, 'tdg.accs.adfa.numCycles', 0.0)
        if cpu_cycles == 0.0:
            print("Warning cpu cycles is 0 for " + region)
            return 1.0
        return adfa_cycles / cpu_cycles

    @staticmethod
    def analyze_adfa(benchmarks):
        regions = dict()
        for b in benchmarks:
            name = b.get_name()
            baseline = Gem5RegionStats(name, b.get_result('replay'))
            adfa = Gem5RegionStats(name, b.get_result('adfa'))
            simd = Gem5RegionStats(name, b.get_result('simd'))

            regions[name] = list()

            print(name)

            for region in baseline.regions:
                print('  ' + region)

                region_name = name + '::' + region

                region_agg_stats = RegionAggStats(
                    ADFAAnalyzer.SYS_CPU_PREFIX, region, region_name)
                region_agg_stats.compute_baseline(baseline)
                region_agg_stats.compute_adfa(adfa)
                region_agg_stats.compute_simd(simd)

                regions[name].append(region_agg_stats)

            # Sort all the regions by their number of dynamic insts.
            regions[name] = sorted(
                regions[name], key=lambda x: x.base_insts, reverse=True)

        baseline.print_regions()
        RegionAggStats.print_title()
        for benchmark_name in regions:
            for region in regions[benchmark_name]:
                region.print_line()
