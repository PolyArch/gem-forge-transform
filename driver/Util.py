
import multiprocessing
import logging
import os
import subprocess
import sys
import unittest
import tempfile
import time
import traceback


def call_helper(cmd, stdout=None):
    """
    Helper function to call a command and print the actual command when failed.
    """
    print(' '.join(cmd))
    try:
        subprocess.check_call(cmd, stdout=stdout)
    except subprocess.CalledProcessError as e:
        print('Error when executing {cmd}'.format(cmd=' '.join(cmd)))
        raise e


def mkdir_p(path):
    if os.path.isdir(path):
        return
    call_helper(['mkdir', '-p', path])


def mkdir_chain(path):
    if os.path.isdir(path):
        return
    parent, child = os.path.split(path)
    mkdir_chain(parent)
    mkdir_p(path)


def create_symbolic_link(src, dest):
    if os.path.exists(dest):
        return
    if os.path.realpath(dest) != src:
        call_helper(['rm', '-f', dest])
    call_helper([
        'ln',
        '-s',
        src,
        dest,
    ])


def error(msg, *args):
    print(msg)
    logger = multiprocessing.get_logger()
    logger.setLevel(logging.WARNING)
    logger.error(msg, *args)


class LogExceptions(object):
    def __init__(self, callable):
        self.__callable = callable

    def __call__(self, *args, **kwargs):
        try:
            result = self.__callable(*args, **kwargs)

        except Exception as e:
            error(traceback.format_exc())
            # Reraise the original exception so the Pool worker can clean up
            raise ValueError('Job Failed.')

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
        self.dump(self.log_f)
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

    def run(self):
        assert(self.state == JobScheduler.STATE_INIT)
        self.log_f = tempfile.NamedTemporaryFile(
            prefix='job_scheduler.{n}.'.format(n=self.name), delete=False)
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
        self.dump(sys.stdout)
        self.dump(self.log_f)
        self.pool.close()
        self.log_f.close()
        self.pool.join()


def test_job(id):
    print('job {job} executed'.format(job=id))


def test_job_fail(id):
    print('job {job} failed'.format(job=id))
    raise ValueError('Job deliberately failed.')


def test_job_fail_bash(id):
    print('job {job} failed in bash'.format(job=id))
    call_helper(['false'])


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

    # Simple linear dependence when the middle one failed in bash.
    def test_linear_fail_bash(self):
        scheduler = JobScheduler(4, 1)
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


if __name__ == '__main__':
    unittest.main()
