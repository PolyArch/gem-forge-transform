
import matplotlib
import matplotlib.pyplot as plt
import numpy
import multiprocessing
import subprocess
import unittest
import time
import traceback


def error(msg, *args):
    return multiprocessing.get_logger().error(msg, *args)


class LogExceptions(object):
    def __init__(self, callable):
        self.__callable = callable

    def __call__(self, *args, **kwargs):
        try:
            result = self.__callable(*args, **kwargs)

        except Exception as e:
            error(traceback.format_exc())
            # Reraise the original exception so the Pool worker can clean up
            raise

        return result


class JobScheduler:
    """
    A job scheduler.
    Since we use pool class, which use a queue.Queue to pass tasks to the worker
    processes. Everything that goes through queue.Queue must be picklable. 
    Pickle is a serialization protocol for python. ONLY TOP-LEVEL object can be
    pickled!
    This means only top-level function for the job, and no nested class object for
    the parameter.
    """
    STATE_INIT = 1
    STATE_STARTED = 2
    STATE_FINISHED = 3
    STATE_FAILED = 4

    class Job:
        def __init__(self, name, job, args):
            self.name = name
            self.job = job
            self.args = args
            self.status = JobScheduler.STATE_INIT

    def __init__(self, cores, poll_seconds):
        self.state = JobScheduler.STATE_INIT
        self.cores = cores
        self.poll_seconds = poll_seconds
        self.current_job_id = 1
        self.lock = multiprocessing.Lock()
        self.pool = multiprocessing.Pool(processes=self.cores)
        self.jobs = dict()
        self.job_children = dict()
        self.job_deps = dict()

        multiprocessing.log_to_stderr()

    # Add a job and set the dependence graph.
    def add_job(self, name, job, args, deps):
        assert(self.state == JobScheduler.STATE_INIT)
        new_job_id = self.current_job_id
        self.current_job_id += 1
        self.job_deps[new_job_id] = list(deps)
        for dep in deps:
            assert(dep in self.jobs)
            self.job_children[dep].append(new_job_id)
        self.job_children[new_job_id] = list()
        self.jobs[new_job_id] = self.Job(name, job, args)
        return new_job_id

    # Caller should hold the locker.
    def kick(self, job_id):
        assert(job_id in self.jobs)
        job = self.jobs[job_id]
        if job.status != JobScheduler.STATE_INIT:
            return
        ready = True
        for dep in self.job_deps[job_id]:
            if self.jobs[dep].status != JobScheduler.STATE_FINISHED:
                ready = False
                break
        if ready:
            # Do we actually need to use nested lambda to capture job_id?
            job.status = JobScheduler.STATE_STARTED
            print('{job} started'.format(job=job_id))
            job.res = self.pool.apply_async(
                func=LogExceptions(job.job),
                args=job.args,
                callback=(lambda i: lambda _: self.call_back(i))(job_id)
            )

    def call_back(self, job_id):
        """
        Callback when the job is done.
        Caller should not hold the lock.
        """
        print('{job} finished'.format(job=job_id))
        self.lock.acquire()
        assert(job_id in self.jobs)
        job = self.jobs[job_id]
        assert(job.status == JobScheduler.STATE_STARTED)
        job.status = JobScheduler.STATE_FINISHED
        for child in self.job_children[job_id]:
            child_job = self.jobs[child]
            assert(child_job.status == JobScheduler.STATE_INIT or child_job.status ==
                   JobScheduler.STATE_FAILED)
            self.kick(child)
        self.lock.release()

    def failed(self, job_id):
        """
        Mark the job and all its descent job Failed.
        Caller should not hold the lock.
        """
        self.lock.acquire()
        assert(job_id in self.jobs)
        job = self.jobs[job_id]
        assert(job.status == JobScheduler.STATE_STARTED)
        stack = list()
        stack.append(job_id)
        while stack:
            job_id = stack.pop()
            print('{job} failed'.format(job=job_id))
            job = self.jobs[job_id]
            job.status = JobScheduler.STATE_FAILED
            for child_id in self.job_children[job_id]:
                stack.append(child_id)
        self.lock.release()

    def str_status(self, status):
        if status == JobScheduler.STATE_INIT:
            return 'INIT'
        if status == JobScheduler.STATE_FAILED:
            return 'FAILED'
        if status == JobScheduler.STATE_STARTED:
            return 'STARTED'
        if status == JobScheduler.STATE_FINISHED:
            return 'FINISHED'
        return 'UNKNOWN'

    def dump(self):
        stack = list()
        for job_id in self.jobs:
            if len(self.job_deps[job_id]) == 0:
                stack.append((job_id, 0))
        print('=================== Job Scheduler =====================')
        while stack:
            job_id, level = stack.pop()
            job = self.jobs[job_id]
            print('{tab}{job_id} {job_name} {status}'.format(
                tab='  '*level,
                job_id=job_id,
                job_name=job.name,
                status=self.str_status(self.jobs[job_id].status)
            ))
            for child_id in self.job_children[job_id]:
                stack.append((child_id, level + 1))
        print('=================== Job Scheduler =====================')

    def run(self):
        assert(self.state == JobScheduler.STATE_INIT)
        self.state = JobScheduler.STATE_STARTED
        self.lock.acquire()
        for job_id in self.job_deps:
            self.kick(job_id)
        self.lock.release()
        # Poll every n seconds.
        while True:
            time.sleep(self.poll_seconds)
            finished = True
            # Try to get the res.
            for job_id in self.jobs:
                job = self.jobs[job_id]
                if job.status == JobScheduler.STATE_STARTED:
                    if job.res.ready() and (not job.res.successful()):
                        # The job failed.
                        self.failed(job_id)

            self.lock.acquire()
            for job_id in self.jobs:
                job = self.jobs[job_id]
                if (job.status != JobScheduler.STATE_FINISHED and job.status != JobScheduler.STATE_FAILED):
                    # print('{job} is not finished'.format(job=job_id))
                    finished = False
                    break
            self.lock.release()
            if finished:
                self.state = JobScheduler.STATE_FINISHED
                break
        self.dump()
        self.pool.close()
        self.pool.join()


def test_job(id):
    print('job {job} executed'.format(job=id))


def test_job_fail(id):
    print('job {job} failed'.format(job=id))
    raise ValueError('Job deliberately failed.')


class TestJobScheduler(unittest.TestCase):

    # Test all jobs are finished.
    # python unittest framework will treat test_* functions as unit test.
    # So make this helper function as assert_*
    def assert_all_jobs_status(self, scheduler, status):
        for job_id in scheduler.jobs:
            job = scheduler.jobs[job_id]
            self.assertEqual(
                job.status, status, 'There is job {job} with wrong status.'.format(job=job_id))

    # Simple test case to test linear dependence.
    def test_linear(self):
        scheduler = JobScheduler(4, 1)
        deps = []
        for i in xrange(8):
            new_job_id = scheduler.add_job('test_job', test_job, (i,), deps)
            deps = list()
            deps.append(new_job_id)
        scheduler.run()
        self.assert_all_jobs_status(scheduler, JobScheduler.STATE_FINISHED)
        self.assertEqual(scheduler.state, JobScheduler.STATE_FINISHED)

    # Simple linear dependence when everything failed.
    def test_linear_fail(self):
        scheduler = JobScheduler(4, 1)
        deps = []
        for i in xrange(8):
            new_job_id = scheduler.add_job(
                'test_job_fail', test_job_fail, (i,), deps)
            deps = list()
            deps.append(new_job_id)
        scheduler.run()
        self.assert_all_jobs_status(scheduler, JobScheduler.STATE_FAILED)
        self.assertEqual(scheduler.state, JobScheduler.STATE_FINISHED)


if __name__ == '__main__':
    unittest.main()


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
