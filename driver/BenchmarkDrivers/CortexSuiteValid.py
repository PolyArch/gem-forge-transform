import Util
import Constants as C

from Utils import TraceFlagEnum

from CortexSuite import CortexBenchmark

import os


class CortexValidBenchmark(CortexBenchmark):
    def __init__(self, benchmark_args, folder, benchmark_name):
        super(CortexValidBenchmark, self).__init__(
            benchmark_args, folder, benchmark_name, 'medium', 'cortex.valid')
        self.defines['GEM_FORGE_VALID'] = None

    # For validation, we have to link with the gem5_pseudo.cpp.
    def get_links(self):
        return ['-lm', os.path.join(self.top_folder, 'gem5_pseudo.cpp')]

    # For validation, trace the special function gem_forge_work.
    def get_trace_func(self):
        return 'gem_forge_work'

    def trace(self):
        # For validation, trace the whole work.
        os.chdir(self.get_exe_path())
        self.build_trace(
            link_stdlib=False,
            trace_reachable_only=False,
        )

        # Fractal experiments.
        os.putenv('LLVM_TDG_TRACE_MODE', str(TraceFlagEnum.GemForgeTraceMode.TraceAll.value))
        os.putenv('LLVM_TDG_TRACE_ROI', str(TraceFlagEnum.GemForgeTraceROI.SpecifiedFunction.value))

        self.run_trace(self.get_name())
        os.chdir(self.cwd)

    def get_additional_gem5_simulate_command(self):
        # For validation, we disable cache warm.
        return ['--gem-forge-cold-cache']

    def build_validation(self, transform_config, trace, output_tdg):
        # Build the validation binary.
        output_dir = os.path.dirname(output_tdg)
        binary = os.path.join(output_dir, self.get_valid_bin())
        build_cmd = [
            C.CC,
            '-static',
        ]
        build_cmd += ['-' + flag for flag in self.flags]
        build_cmd += [
            self.get_raw_bc(),
            # Link with gem5.
            '-I{gem5_include}'.format(gem5_include=C.GEM5_INCLUDE_DIR),
            C.GEM5_M5OPS_X86,
            '-lm',
            '-o',
            binary
        ]
        Util.call_helper(build_cmd)
        asm = os.path.join(output_dir, self.get_name() + '.asm')
        with open(asm, 'w') as f:
            disasm_cmd = [
                'objdump',
                '-d',
                binary
            ]
            Util.call_helper(disasm_cmd, stdout=f)

    def simulate_valid(self, tdg, transform_config, simulation_config):
        print("# Simulating the validation binary.")
        os.chdir(self.work_path)
        gem5_out_dir = simulation_config.get_gem5_dir(tdg)
        tdg_dir = os.path.dirname(tdg)
        binary = os.path.join(tdg_dir, self.get_valid_bin())
        gem5_args = self.get_gem5_simulate_command(
            simulation_config=simulation_config,
            binary=binary,
            outdir=gem5_out_dir,
            standalone=False,
        )
        # Exit immediately when we are done.
        gem5_args.append('--work-end-exit-count=1')
        # Do not add the fake tdg file, so that the script in gem5 will
        # actually simulate the valid bin.
        Util.call_helper(gem5_args)
        os.chdir(self.cwd)


class CortexValidSuite:

    def __init__(self, benchmark_args):
        folder = os.getenv('CORTEX_SUITE_PATH')
        assert(folder is not None)
        self.benchmarks = list()
        for benchmark_name in [
            'srr',
            'rbm',
            'motion-estimation',
            'sphinx',
            'pca',
            'svd3',
            'liblinear',
        ]:
            benchmark = CortexValidBenchmark(
                benchmark_args, folder, benchmark_name)
            self.benchmarks.append(benchmark)

    def get_benchmarks(self):
        return self.benchmarks
