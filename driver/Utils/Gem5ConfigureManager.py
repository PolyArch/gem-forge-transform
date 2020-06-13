import Constants as C
import os
import json
import re
import copy
import math

def log2Byte(v):
    if v[-2] == 'k':
        return int(math.log(int(v[:-2]), 2)) + 10
    elif v[-2] == 'M':
        return int(math.log(int(v[:-2]), 2)) + 20
    else:
        return int(math.log(int(v[:-1]), 2))

class Gem5ReplayConfig(object):

    def __init__(self, json):
        self.json = json

    def get_simulation_id(self):
        return self.json['id']

    def get_support_transform_ids(self):
        return self.json['support-transform-ids']

    def get_gem5_dir(self, tdg, sim_input_name=None):
        dirname, basename = os.path.split(tdg)
        gem5_dir = os.path.join(dirname, self.get_simulation_id(), basename)
        if sim_input_name:
            gem5_dir = '{d}.{i}'.format(d=gem5_dir, i=sim_input_name)
        return gem5_dir

    def get_result(self, tdg):
        _, basename = os.path.split(tdg)
        return os.path.join(
            C.LLVM_TDG_RESULT_DIR,
            '{config}.txt'.format(
                config=self.get_config(basename),
            ))

    """
    
            '--llvm-issue-width={iw}'.format(iw=self.issue_width)
            '--l1d_size={l1d}'.format(l1d=self.options.l1d_size)
            '--l1d_lat={l1d}'.format(l1d=self.options.l1d_latency)
            '--l1d_mshrs={l1d_mshrs}'.format(l1d_mshrs=self.l1d_mshrs)
            '--l1d_assoc={l1d_assoc}'.format(l1d_assoc=self.l1d_assoc),
            '--l2_size={l2}'.format(l2=self.options.l2_size)
            '--l2_lat={l2}'.format(l2=self.options.l2_latency)
            '--l2_mshrs={l2}'.format(l2=self.options.l2_mshrs)
            '--l2_assoc={l2}'.format(l2=self.options.l2_assoc)
            '--l1_5d_mshrs={l1_5d_mshrs}'.format(l1_5d_mshrs=self.l1_5d_mshrs)
            '--l2bus_width={l2bus_width}'.format(l2bus_width=self.l2bus_width)
            '--l1_5dcache'
            '--llvm-prefetch=1'
            '--gem-forge-adfa-enable-speculation=1'
            '--gem-forge-adfa-break-iv-dep=1'
            '--gem-forge-adfa-break-rv-dep=1'
            '--gem-forge-stream-engine-is-oracle=1'
            '--gem-forge-stream-engine-max-run-ahead-length={x}'.format(
                x=self.stream_engine_max_run_ahead_length
            )
            '--gem-forge-stream-engine-throttling={x}'.format(
                x=self.stream_engine_throttling
            )
            '--gem-forge-stream-engine-enable-coalesce=1'
            '--gem-forge-stream-engine-enable-merge=1'
            '--gem-forge-stream-engine-l1d={l1d}'.format(
                l1d=self.stream_engine_l1d)
    """

    def get_options(self):
        return self.json['options']

    def get_config(self, tdg_basename):
        return self.get_config_id('whatever', tdg_basename)

    def get_config_id(self, transform, prefix='config'):
        return '{prefix}.{id}'.format(prefix=prefix, id=self.json['id'])


"""
GemForgeSystemConfig is simply a tuple of TransformConfig and Gem5ReplayConfig.
Gem5ReplayConfigureManager will host all legal GemForgeSystemConfig.
"""


class GemForgeSystemConfig(object):
    def __init__(self, transform_config, simulation_config):
        self.transform_config = transform_config
        self.simulation_config = simulation_config

    def get_id(self):
        return '{transform_id}/{simulation_id}'.format(
            transform_id=self.transform_config.get_transform_id(),
            simulation_id=self.simulation_config.get_simulation_id(),
        )


class Gem5ReplayConfigureManager(object):
    def __init__(self, simulation_fns, transform_manager):
        self.transform_manager = transform_manager
        self.configs = dict()
        self.gem_forge_system_configs = list()
        self.field_re = re.compile('\{[^\{\}]+\}')
        self.simulation_folder_root = os.path.join(
            C.LLVM_TDG_DRIVER_DIR, 'Configurations/Simulation')

        # Allocate a list for every transform configuration.
        for t in self.transform_manager.get_all_transform_ids():
            self.configs[t] = list()

        for fn in simulation_fns:
            # Make sure fn is contained within the simulation folder.
            assert(os.path.commonprefix([fn, self.simulation_folder_root])
                   == self.simulation_folder_root)
            with open(fn, 'r') as f:
                json_obj = json.load(f)
                assert('options' in json_obj)
                # Generate the id from folder.fn if there is no id.
                relative_folder = os.path.dirname(
                    os.path.relpath(fn, self.simulation_folder_root))
                id_prefix = relative_folder.replace(os.sep, '.')
                try:
                    id_body = json_obj['id']
                except KeyError:
                    id_body = os.path.basename(fn)[:-5]
                
                json_obj['id'] = '{prefix}.{body}'.format(
                    prefix=id_prefix,
                    body=id_body,
                )
            if 'snippts' in json_obj:
                json_obj = self.insert_snippts(json_obj)
            if 'design_space' in json_obj:
                configs = self.generate_config_for_design_space(json_obj)
            else:
                configs = [Gem5ReplayConfig(json_obj)]
            for config in configs:
                support_transforms = config.get_support_transform_ids()
                for t in support_transforms:
                    if t in self.configs:
                        self.configs[t].append(config)
                        self.gem_forge_system_configs.append(GemForgeSystemConfig(
                            self.transform_manager.get_config(t), config))
                if not support_transforms:
                    # For empty support transform list, we assume it can support all
                    # transformation.
                    for t in self.configs:
                        self.configs[t].append(config)
                        self.gem_forge_system_configs.append(GemForgeSystemConfig(
                            self.transform_manager.get_config(t), config))

    def replace_field(self, match, design):
        expression = match.group(0)[1:-1]
        env = {
            'log2Byte': log2Byte,
        }
        evaluated = eval(expression, env, design)
        return str(evaluated)

    def generate_config_for_design_space_recursive(
        self, json, vars, var_idx, design, configs):
        design_space = json['design_space']
        if var_idx == len(vars):
            # We are done, expand the json.
            expanded_json = copy.deepcopy(json)
            expanded_json['id'] = self.field_re.sub(
                lambda match: self.replace_field(match, design),
                expanded_json['id'])
            for option_i in range(len(expanded_json['options'])):
                expanded_json['options'][option_i] = self.field_re.sub(
                    lambda match: self.replace_field(match, design),
                    expanded_json['options'][option_i]
                )
            configs.append(Gem5ReplayConfig(expanded_json))
        else:
            # Keep building up the design.
            var = vars[var_idx]
            for value in design_space[var]:
                design[var] = value
                self.generate_config_for_design_space_recursive(
                    json, vars, var_idx + 1, design, configs
                )

    def generate_config_for_design_space(self, json):
        design_space = json['design_space']
        configs = list()
        self.generate_config_for_design_space_recursive(
            json, design_space.keys(), 0, dict(), configs
        )
        return configs

    def get_configs(self, transform_id):
        return self.configs[transform_id]

    def get_all_gem_forge_system_configs(self):
        return self.gem_forge_system_configs

    def insert_snippts(self, json_obj):
        for snippt_name in json_obj['snippts']:
            snippt = Gem5ReplayConfigureManager.SNIPPTS[snippt_name]
            json_obj['options'] += snippt
        return json_obj

    """
    Common snippts.
    """
    L2_TLB = [
        '--l1tlb-size=64',
        '--l1tlb-assoc=8',
        '--l2tlb-size=2048',
        '--l2tlb-assoc=16',
        '--l2tlb-hit-lat=8',
        '--walker-se-lat=16',
        '--walker-se-port=2',
    ]
    LLC_256kB = [
        '--l2_size=256kB',
        '--l2_assoc=16',
    ]
    LLC_1MB = [
        '--l2_size=1MB',
        '--l2_assoc=16',
    ]
    LLC_4MB = [
        '--l2_size=4MB',
        '--l2_assoc=16',
    ]
    LLC_SELECT_4kB = [
        '--llc-select-low-bit=12',
    ]
    LLC_SELECT_2kB = [
        '--llc-select-low-bit=11',
    ]
    LLC_SELECT_1kB = [
        '--llc-select-low-bit=10',
    ]
    LLC_SELECT_512B = [
        '--llc-select-low-bit=9',
    ]
    LLC_SELECT_256B = [
        '--llc-select-low-bit=8',
    ]
    LLC_SELECT_128B = [
        '--llc-select-low-bit=7',
    ]
    LLC_SELECT_64B = [
        '--llc-select-low-bit=6',
    ]
    L0_32kB = [
        '--l1i_size=32kB',
        '--l1i_assoc=8',
        '--l1d_size=32kB',
        '--l1d_lat=8',
        '--l1d_mshrs=8',
        '--l1d_assoc=8',
    ]
    L0_32MB = [
        '--l1i_size=32MB',
        '--l1i_assoc=8',
        '--l1d_size=32MB',
        '--l1d_lat=8',
        '--l1d_mshrs=8',
        '--l1d_assoc=8',
    ]
    MLC_256kB = [
        '--l1_5d_size=256kB',
        '--l1_5d_assoc=16',
        '--l1_5d_mshrs=16',
    ]
    MLC_64kB = [
        '--l1_5d_size=64kB',
        '--l1_5d_assoc=16',
        '--l1_5d_mshrs=16',
    ]
    MLC_32MB = [
        '--l1_5d_size=32MB',
        '--l1_5d_assoc=16',
        '--l1_5d_mshrs=16',
    ]
    RUBY_L3_BASE = [
        "--ruby",
        "--access-backing-store",
        "--network=garnet2.0",
        "--router-latency=2",
        "--link-latency=1",
        "--mem-channels=2",
        "--mem-size=16GB",
    ]
    RUBY_MESH = [
        "--topology=Mesh_XY",
    ]
    RUBY_MESH_DIR_CORNER = [
        "--topology=MeshDirCorners_XY",
        "--routing-YX", # Routing in YX direction.
    ]
    RUBY_L3 = RUBY_L3_BASE + RUBY_MESH
    RUBY_L3_DIR_CORNER = RUBY_L3_BASE + RUBY_MESH_DIR_CORNER
    
    SNIPPTS = {
        '4x4.dir_corner.l3.4MB.ruby': [
            "--num-cpus=16",
            "--num-dirs=4",
            "--num-l2caches=16",
            "--mesh-rows=4",
        ] + RUBY_L3_DIR_CORNER + L0_32kB + MLC_256kB + LLC_4MB + LLC_SELECT_4kB,
        '4x4.dir_corner.l2_256kB.l3_1MB.ruby': [
            "--num-cpus=16",
            "--num-dirs=4",
            "--num-l2caches=16",
            "--mesh-rows=4",
        ] + RUBY_L3_DIR_CORNER + L0_32kB + MLC_256kB + LLC_1MB + LLC_SELECT_4kB,
        '4x8.dir_corner.l2_256kB.l3_1MB.ruby': [
            "--num-cpus=32",
            "--num-dirs=4",
            "--num-l2caches=32",
            "--mesh-rows=4",
        ] + RUBY_L3_DIR_CORNER + L0_32kB + MLC_256kB + LLC_1MB + LLC_SELECT_4kB,
        # Ideal means large private cache.
        '4x4.l3.idea.ruby': [
            "--num-cpus=16",
            "--num-dirs=16",
            "--num-l2caches=16",
            "--mesh-rows=4",
        ] + RUBY_L3 + L0_32MB + MLC_32MB + LLC_4MB + LLC_SELECT_4kB,
        '8x8.dir_corner.l3.1MB.ruby': [
            "--num-cpus=64",
            "--num-dirs=4",
            "--num-l2caches=64",
            "--mesh-rows=8",
        ] + RUBY_L3_DIR_CORNER + L0_32kB + MLC_64kB + LLC_1MB + LLC_SELECT_4kB,
        '8x8.dir_corner.l2_256kB.l3_1MB.ruby': [
            "--num-cpus=64",
            "--num-dirs=4",
            "--num-l2caches=64",
            "--mesh-rows=8",
        ] + RUBY_L3_DIR_CORNER + L0_32kB + MLC_256kB + LLC_1MB + LLC_SELECT_4kB,
        '8x8.dir_corner.l2_256kB.l3_1MB_s2kB.ruby': [
            "--num-cpus=64",
            "--num-dirs=4",
            "--num-l2caches=64",
            "--mesh-rows=8",
        ] + RUBY_L3_DIR_CORNER + L0_32kB + MLC_256kB + LLC_1MB + LLC_SELECT_2kB,
        '8x8.dir_corner.l2_256kB.l3_1MB_s1kB.ruby': [
            "--num-cpus=64",
            "--num-dirs=4",
            "--num-l2caches=64",
            "--mesh-rows=8",
        ] + RUBY_L3_DIR_CORNER + L0_32kB + MLC_256kB + LLC_1MB + LLC_SELECT_1kB,
        '4x4.dir_corner.l2_256kB.l3_1MB_s0.ruby': [
            "--num-cpus=16",
            "--num-dirs=4",
            "--num-l2caches=16",
            "--mesh-rows=4",
        ] + RUBY_L3_DIR_CORNER + L0_32kB + MLC_256kB + LLC_1MB,
        '4x8.dir_corner.l2_256kB.l3_1MB_s0.ruby': [
            "--num-cpus=32",
            "--num-dirs=4",
            "--num-l2caches=32",
            "--mesh-rows=4",
        ] + RUBY_L3_DIR_CORNER + L0_32kB + MLC_256kB + LLC_1MB,
        '8x8.dir_corner.l2_256kB.l3_1MB_s0.ruby': [
            "--num-cpus=64",
            "--num-dirs=4",
            "--num-l2caches=64",
            "--mesh-rows=8",
        ] + RUBY_L3_DIR_CORNER + L0_32kB + MLC_256kB + LLC_1MB,
        'llc_select_512B': LLC_SELECT_512B,
        'llc_select_256B': LLC_SELECT_256B,
        'llc_select_128B': LLC_SELECT_128B,
        'llc_select_64B': LLC_SELECT_64B,
        'o8': [
            "--cpu-type=DerivO3CPU",
            "--llvm-issue-width=8",
        ],
        'o4': [
            "--cpu-type=DerivO3CPU",
            "--llvm-issue-width=4",
            "--prog-interval=10000", # Hz
        ] + L2_TLB,
        'o4.tlb': [
            "--cpu-type=DerivO3CPU",
            "--llvm-issue-width=4",
            "--prog-interval=10000", # Hz
            "--tlb-timing-se",
        ] + L2_TLB,
        'i2': [
            "--cpu-type=MinorCPU",
            "--llvm-issue-width=2",
            "--prog-interval=1000",
        ],
        'i4': [
            "--cpu-type=MinorCPU",
            "--llvm-issue-width=4",
            "--prog-interval=10000", # Hz
        ] + L2_TLB,
        'i4.tlb': [
            "--cpu-type=MinorCPU",
            "--llvm-issue-width=4",
            "--prog-interval=10000", # Hz
            "--tlb-timing-se",
        ] + L2_TLB,
    }
