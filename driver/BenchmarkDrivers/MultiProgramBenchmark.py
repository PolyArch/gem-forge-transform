import Util
import Constants as C
from Benchmark import Benchmark

from pprint import pprint
import os


class MultiProgramTDG(object):

    def __init__(self, benchmark, idx, transform_config, tdgs):
        self.benchmark = benchmark
        self.idx = idx
        self.transform_config = transform_config
        self.tdgs = tdgs

    def get_name(self):
        return '{transform_path}/{name}.{transform_id}.{trace_id}.tdg'.format(
            transform_path=self.benchmark.get_transform_path(
                self.transform_config.get_transform_id()),
            name=self.benchmark.get_name(),
            transform_id=self.transform_config.get_transform_id(),
            trace_id=self.idx
        )


class MultiProgramBenchmark(Benchmark):
    def __init__(self, benchmark_args, benchmarks):
        self.benchmarks = benchmarks
        self.name = '.'.join([b.get_name() for b in self.benchmarks])
        self.work_path = os.path.join(
            C.LLVM_TDG_RESULT_DIR, 'multi', self.name)
        Util.mkdir_chain(self.work_path)

        self.transform_config_to_tdgs = dict()
        self.tdg_map = dict()

        super(MultiProgramBenchmark, self).__init__(
            benchmark_args
        )

    def get_name(self):
        return self.name

    def get_links(self):
        raise NotImplementedError(
            "MultiProgramBenchmark only supports simulation.")

    def get_args(self):
        raise NotImplementedError(
            "MultiProgramBenchmark only supports simulation.")

    def get_trace_func(self):
        raise NotImplementedError(
            "MultiProgramBenchmark only supports simulation.")

    def get_lang(self):
        raise NotImplementedError(
            "MultiProgramBenchmark only supports simulation.")

    def get_exe_path(self):
        raise NotImplementedError(
            "MultiProgramBenchmark only supports simulation.")

    def get_run_path(self):
        return self.work_path

    def get_raw_bc(self):
        raise NotImplementedError(
            "MultiProgramBenchmark only supports simulation.")

    def build_raw_bc(self):
        raise NotImplementedError(
            "MultiProgramBenchmark only supports simulation.")

    def profile(self):
        raise NotImplementedError(
            "MultiProgramBenchmark only supports simulation.")

    def simpoint(self):
        raise NotImplementedError(
            "MultiProgramBenchmark only supports simulation.")

    def trace(self):
        raise NotImplementedError(
            "MultiProgramBenchmark only supports simulation.")

    def transform(self, transform_config, trace, tdg, debugs):
        raise NotImplementedError(
            "MultiProgramBenchmark only supports simulation.")

    """
    Virtual TDGs.
    """

    def get_tdgs(self, transform_config):
        if transform_config in self.transform_config_to_tdgs:
            return self.transform_config_to_tdgs[transform_config]

        tdgs = list()

        transform_id = transform_config.get_transform_id()
        benchmark_tdgs = [b.get_tdgs(transform_config)
                          for b in self.benchmarks]
        # Take the min of number of all benchmarks' tdgs.
        total_virtual_tdgs = min([len(d) for d in benchmark_tdgs])
        for virtual_tdg_idx in xrange(total_virtual_tdgs):
            single_tdgs = [b[virtual_tdg_idx] for b in benchmark_tdgs]
            multi_program_tdg = MultiProgramTDG(
                self, virtual_tdg_idx, transform_config, single_tdgs)

            # Add to the map
            self.tdg_map[multi_program_tdg.get_name()] = multi_program_tdg
            tdgs.append(multi_program_tdg.get_name())

        # Hack: for now only one job.
        # tdgs = tdgs[0:1]
        self.transform_config_to_tdgs[transform_config] = tdgs
        return tdgs

    def simulate(self, tdg, simulation_config):
        if tdg not in self.tdg_map:
            raise ValueError("MultiProgramTDG not found.")
        multi_program_tdg = self.tdg_map[tdg]

        sim_out_dir = simulation_config.get_gem5_dir(tdg)
        Util.mkdir_p(sim_out_dir)
        gem5_args = self.get_gem5_simulate_command(
            simulation_config, self.get_replay_bin, sim_out_dir, False)
        # Remember to add back all the tdgs.
        gem5_args.append(
            '--llvm-trace-file={tdg}'.format(
                tdg=','.join(multi_program_tdg.tdgs))
        )
        print('# Simulate multi-program datagraph.')
        pprint(gem5_args)
        Util.call_helper(gem5_args)
        
