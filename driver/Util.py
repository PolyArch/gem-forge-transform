
import os
import subprocess


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


def mkdir_f(path):
    if os.path.isdir(path):
        call_helper(['rm', '-r', path])
    mkdir_p(path)


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
