import Util
import Constants as C
from Benchmark import Benchmark

from pprint import pprint
import os


class MultiProgramTrace(object):

    def __init__(self, benchmark, trace_id, single_program_traces):
        self.benchmark = benchmark
        self.trace_id = trace_id
        self.single_program_traces = single_program_traces

    def get_trace_id(self):
        return self.trace_id

    def get_single_program_tdgs(self, transform_config):
        tdgs = list()
        for single_benchmark, trace in self.single_program_traces:
            tdgs.append(single_benchmark.get_tdg(transform_config, trace))
        return tdgs

    def get_name(self):
        return '{transform_path}/{name}.{transform_id}.{trace_id}.tdg'.format(
            transform_path=self.benchmark.get_transform_path(
                self.transform_config.get_transform_id()),
            name=self.benchmark.get_name(),
            transform_id=self.transform_config.get_transform_id(),
            trace_id=self.idx
        )

    def get_weight(self):
        return 1.0


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

        # Build MultiProgramTrace upon single program traces.
        self.build_multi_program_traces()

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

    def transform(self, transform_config, trace, tdg):
        raise NotImplementedError(
            "MultiProgramBenchmark only supports simulation.")

    def build_multi_program_traces(self):
        # Initially we should have no traces.
        assert(not self.traces)
        total_multi_program_traces = min(
            [len(b.get_traces()) for b in self.benchmarks])
        for trace_id in xrange(total_multi_program_traces):
            single_program_traces = [
                (b, b.get_traces()[trace_id])
                for b in self.benchmarks]
            multi_program_trace = MultiProgramTrace(
                self, trace_id, single_program_traces)
            self.traces.append(multi_program_trace)

    def simulate(self, trace, transform_config, simulation_config):
        tdg = self.get_tdg(transform_config, trace)
        sim_out_dir = simulation_config.get_gem5_dir(tdg)
        Util.mkdir_p(sim_out_dir)
        gem5_args = self.get_gem5_simulate_command(
            simulation_config, self.get_replay_bin, sim_out_dir, False)
        # Remember to add back all the tdgs.
        gem5_args.append(
            '--llvm-trace-file={tdgs}'.format(
                tdgs=','.join(trace.get_single_program_tdgs(
                    transform_config)))
        )
        print('# Simulate multi-program datagraph.')
        pprint(gem5_args)
        Util.call_helper(gem5_args)
