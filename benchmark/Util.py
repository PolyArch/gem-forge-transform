
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
