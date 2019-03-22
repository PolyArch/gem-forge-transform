import Utils.InstructionUIDMapReader

import os


class PerfInstEntry(object):
    def __init__(self, line):
        fields = line.split()
        self.shared_obj = fields[2]
        self.func_name = fields[4]
        self.insts = float(fields[0].strip('%'))
        self.is_kernel = fields[3] == '[k]'


class PerfInstReader(object):
    def __init__(self, fn):
        self.func_entry_map = dict()
        self.entry_list = list()
        with open(fn) as f:
            for line in f:
                if not ('%' in line):
                    continue
                entry = PerfInstEntry(line)
                self.func_entry_map[entry.func_name] = entry
                self.entry_list.append(entry)
        self.entry_list.sort(reverse=True, key=lambda x: x.insts)


class LibraryInstExperiments(object):
    def __init__(self, driver):
        self.driver = driver
        for benchmark in self.driver.benchmarks:
            uid_fn = os.path.join(benchmark.get_run_path(),
                                  benchmark.get_inst_uid())
            uid_reader = Utils.InstructionUIDMapReader.InstructionUIDMapReader(
                uid_fn)
            perf_fn = os.path.join(benchmark.get_run_path(), 'perf.txt')
            if not os.path.isfile(perf_fn):
                print('Failed to open perf file {fn}.'.format(fn=perf_fn))
                continue
            perf_reader = PerfInstReader(perf_fn)
            total_insts = 0.0
            total_kernal_insts = 0.0
            for func_name in perf_reader.func_entry_map:
                entry = perf_reader.func_entry_map[func_name]
                if uid_reader.hasFunction(func_name):
                    total_insts += entry.insts
                if entry.is_kernel:
                    total_kernal_insts += entry.insts
            print('{b:30} traced {total:10} kernel {kernel:10}.'.format(
                b=benchmark.get_name(),
                total=total_insts,
                kernel=total_kernal_insts))
            # Also print the major untraced not kernel function.
            self.printMajorUntraced(benchmark, perf_reader, uid_reader)

    def printMajorUntraced(self, benchmark, perf_reader, uid_reader):
        total = 0.0
        for entry in perf_reader.entry_list:
            total += entry.insts
            if uid_reader.hasFunction(entry.func_name):
                continue
            if entry.is_kernel:
                continue
            print('{b:20} {so:30} {func:30} {insts:10}'.format(
                b=benchmark.get_name(),
                so=entry.shared_obj,
                func=entry.func_name,
                insts=entry.insts,
            ))
            if total > 99.0:
                break


def analyze(driver):
    LibraryInstExperiments(driver)
