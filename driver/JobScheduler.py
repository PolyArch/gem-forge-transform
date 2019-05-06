
import multiprocessing
import logging
import unittest
import tempfile
import time
import traceback
import threading
import sys


def error(msg, *args):
    print(msg)
    logger = multiprocessing.get_logger()
    logger.setLevel(logging.WARNING)
    logger.error(msg, *args)


class LogExceptions(object):
    def __init__(self, callable, timeout=None):
        self.__callable = callable
        self.__timeout = timeout

    def __call__(self, *args, **kwargs):
        pool = multiprocessing.dummy.Pool(1)
        result = pool.apply_async(self.__callable, args=args, kwds=kwargs)
        try:
            result.get(self.__timeout)
        except multiprocessing.TimeoutError as e:
            raise e
        except Exception as e:
            error(traceback.format_exc())
            # Reraise the original exception so the Pool worker can clean up
            raise ValueError('Job Failed.')


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
    STATE_STARTED = 3
    STATE_FINISHED = 4
    STATE_FAILED = 5
    STATE_TIMEOUTED = STATE_FAILED + 1

    class Job:
        def __init__(self, name, job, args, timeout=None):
            self.name = name
            self.job = job
            self.args = args
            self.status = JobScheduler.STATE_INIT
            self.timeout = timeout

    def __init__(self, name, cores, poll_seconds):
        self.state = JobScheduler.STATE_INIT
        self.name = name
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
    def add_job(self, name, job, args, deps, timeout=None):
        assert(self.state == JobScheduler.STATE_INIT)
        new_job_id = self.current_job_id
        self.current_job_id += 1
        self.job_deps[new_job_id] = list(deps)
        for dep in deps:
            assert(dep in self.jobs)
            self.job_children[dep].append(new_job_id)
        self.job_children[new_job_id] = list()
        self.jobs[new_job_id] = self.Job(name, job, args, timeout)
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
                func=LogExceptions(job.job, job.timeout),
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
        self.dump(self.log_f)
        self.lock.release()

    def failed(self, job_id):
        """
        Mark the job and all its descent job Failed.
        Caller should not hold the lock.
        """
        self.lock.acquire()
        assert(job_id in self.jobs)
        job = self.jobs[job_id]
        assert(job.res.ready() and (not job.res.successful()))
        assert(job.status == JobScheduler.STATE_STARTED)

        # Try to get the exception.
        new_status = JobScheduler.STATE_FAILED
        try:
            job.res.get(timeout=1)
        except multiprocessing.TimeoutError:
            # This is a timeout error.
            new_status = JobScheduler.STATE_TIMEOUTED
        except Exception:
            # Normal failed job.
            new_status = JobScheduler.STATE_FAILED
        else:
            print("Should reraise an exception for unsuccessful job.")
            assert(False)

        job.status = new_status
        stack = list()
        for child_id in self.job_children[job_id]:
            stack.append(job_id)
        while stack:
            job_id = stack.pop()
            print('{job} unsucceeded'.format(job=job_id))
            job = self.jobs[job_id]
            job.status = JobScheduler.STATE_FAILED
            for child_id in self.job_children[job_id]:
                stack.append(child_id)
        self.dump(self.log_f)
        self.dump_failed(self.log_failed_f)
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
        if status == JobScheduler.STATE_TIMEOUTED:
            return 'TIMEOUTED'
        return 'UNKNOWN'

    def dump(self, f):
        stack = list()
        for job_id in self.jobs:
            if len(self.job_deps[job_id]) == 0:
                stack.append((job_id, 0))
        f.write('=================== Job Scheduler =====================\n')
        while stack:
            job_id, level = stack.pop()
            job = self.jobs[job_id]
            f.write('{tab}{job_id} {job_name} {status}\n'.format(
                tab='  '*level,
                job_id=job_id,
                job_name=job.name,
                status=self.str_status(self.jobs[job_id].status)
            ))
            for child_id in self.job_children[job_id]:
                stack.append((child_id, level + 1))
        f.write('=================== Job Scheduler =====================\n')
        f.flush()

    def dump_failed(self, f):
        # Only write the failed job.
        stack = list()
        for job_id in self.jobs:
            if len(self.job_deps[job_id]) == 0:
                stack.append((job_id, 0))
        f.write('=================== Job Scheduler =====================\n')
        while stack:
            job_id, level = stack.pop()
            job = self.jobs[job_id]
            status = self.jobs[job_id].status
            if status > JobScheduler.STATE_STARTED and status != JobScheduler.STATE_FINISHED:
                f.write('{tab}{job_id} {job_name} {status}\n'.format(
                    tab='  '*level,
                    job_id=job_id,
                    job_name=job.name,
                    status=self.str_status(self.jobs[job_id].status)
                ))
            for child_id in self.job_children[job_id]:
                stack.append((child_id, level + 1))
        f.write('=================== Job Scheduler =====================\n')
        f.flush()

    def run(self):
        assert(self.state == JobScheduler.STATE_INIT)
        self.log_f = tempfile.NamedTemporaryFile(
            prefix='job_scheduler.{n}.'.format(n=self.name), delete=False)
        self.log_failed_f = tempfile.NamedTemporaryFile(
            prefix='job_scheduler.{n}.fail.'.format(n=self.name), delete=False)
        print(self.log_f.name)
        seconds = 0

        self.state = JobScheduler.STATE_STARTED
        self.lock.acquire()
        for job_id in self.job_deps:
            self.kick(job_id)
        self.lock.release()
        # Poll every n seconds.
        while True:
            time.sleep(self.poll_seconds)

            seconds += self.poll_seconds
            if seconds > 600:
                seconds = 0
                self.log_f.truncate()
                self.dump(self.log_f)
                self.dump_failed(self.log_failed_f)

            finished = True
            # Try to get the res.
            for job_id in self.jobs:
                job = self.jobs[job_id]
                if job.status == JobScheduler.STATE_STARTED:
                    if job.res.ready() and (not job.res.successful()):
                        self.failed(job_id)

            self.lock.acquire()
            for job_id in self.jobs:
                job = self.jobs[job_id]
                if job.status < JobScheduler.STATE_FINISHED:
                    finished = False
                    break
            self.lock.release()
            if finished:
                self.state = JobScheduler.STATE_FINISHED
                break
        self.dump(sys.stdout)
        self.dump(self.log_f)
        self.dump_failed(self.log_failed_f)

        self.pool.close()
        self.log_f.close()
        self.log_failed_f.close()
        self.pool.join()


def test_job(id):
    print('job {job} executed'.format(job=id))


def test_job_fail(id):
    print('job {job} failed'.format(job=id))
    raise ValueError('Job deliberately failed.')


def test_job_fail_bash(id):
    print('job {job} failed in bash'.format(job=id))
    import subprocess
    print('job {job} failed in bash'.format(job=id))
    subprocess.check_call(['false'])


def test_job_timeout(id, seconds):
    print('job {job} sleeps {s} seconds.'.format(job=id, s=seconds))
    time.sleep(5)


class TestJobScheduler(unittest.TestCase):

    # Test all jobs are finished.
    # python unittest framework will treat test_* functions as unit test.
    # So make this helper function as assert_*
    def assert_all_jobs_status(self, scheduler, status):
        for job_id in scheduler.jobs:
            job = scheduler.jobs[job_id]
            self.assertEqual(
                job.status, status, 'There is job {job} with wrong status.'.format(job=job_id))

    def assert_job_status(self, scheduler, status, job_id):
        job = scheduler.jobs[job_id]
        self.assertEqual(
            job.status, status, 'There is job {job} with wrong status.'.format(job=job_id))

    # Simple test case to test linear dependence.
    def test_linear(self):
        scheduler = JobScheduler('test', 4, 1)
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
        scheduler = JobScheduler('test', 4, 1)
        deps = []
        for i in xrange(8):
            new_job_id = scheduler.add_job(
                'test_job_fail', test_job_fail, (i,), deps)
            deps = list()
            deps.append(new_job_id)
        scheduler.run()
        self.assert_all_jobs_status(scheduler, JobScheduler.STATE_FAILED)
        self.assertEqual(scheduler.state, JobScheduler.STATE_FINISHED)

    # Simple linear dependence when the middle one failed in bash.
    def test_linear_fail_bash(self):
        scheduler = JobScheduler('test', 4, 1)
        deps = []
        job_ids = list()
        failed_id = 0
        for i in xrange(8):
            if i == 4:
                new_job_id = scheduler.add_job(
                    'test_job_fail_bash', test_job_fail_bash, (i,), deps)
                failed_id = new_job_id
            else:
                new_job_id = scheduler.add_job(
                    'test_job', test_job, (i,), deps)
            job_ids.append(new_job_id)
            deps = list()
            deps.append(new_job_id)
        scheduler.run()
        for i in job_ids:
            if i >= failed_id:
                self.assert_job_status(scheduler, JobScheduler.STATE_FAILED, i)
            else:
                self.assert_job_status(
                    scheduler, JobScheduler.STATE_FINISHED, i)
        self.assertEqual(scheduler.state, JobScheduler.STATE_FINISHED)

    def test_timeout_basic(self):
        scheduler = JobScheduler('test', 4, 1)
        scheduler.add_job(
            'test_job_timeout', test_job_timeout, (0, 5), list(), timeout=1)
        scheduler.run()
        self.assert_all_jobs_status(scheduler, JobScheduler.STATE_TIMEOUTED)

    def test_thread_not_quit(self):
        scheduler = JobScheduler('test', 4, 1)
        for i in xrange(8):
            scheduler.add_job(
                'test_job_fail_bash', test_job_fail_bash, (i,), list())
        for i in xrange(8, 16):
            scheduler.add_job(
                'test_job', test_job, (i,), list())
        scheduler.run()
        # Job id starts from 1.
        for i in xrange(1, 9):
            self.assert_job_status(scheduler, JobScheduler.STATE_FAILED, i)
        for i in xrange(9, 17):
            self.assert_job_status(scheduler, JobScheduler.STATE_FINISHED, i)


if __name__ == '__main__':
    unittest.main()
