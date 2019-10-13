import Constants as C
import os
import json
import re
import copy


class Gem5ReplayConfig(object):

    def __init__(self, json):
        self.json = json

    def get_simulation_id(self):
        return self.json['id']

    def get_support_transform_ids(self):
        return self.json['support-transform-ids']

    def get_gem5_dir(self, tdg, sim_input_size=None):
        dirname, basename = os.path.split(tdg)
        gem5_dir = os.path.join(dirname, self.get_simulation_id(), basename)
        if sim_input_size:
            gem5_dir = '{d}.{i}'.format(d=gem5_dir, i=sim_input_size)
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
                # Generate the id from folder.fn
                relative_folder = os.path.dirname(
                    os.path.relpath(fn, self.simulation_folder_root))
                id_prefix = relative_folder.replace(os.sep, '.')
                id_body = os.path.basename(fn)[:-5]
                json_obj['id'] = '{prefix}.{body}'.format(
                    prefix=id_prefix,
                    body=id_body,
                )
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
        evaluated = eval(expression, dict(), design)
        return str(evaluated)

    def generate_config_for_design_space(self, json):
        design_space = json['design_space']
        configs = list()
        for design in design_space:
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
        return configs

    def get_configs(self, transform_id):
        return self.configs[transform_id]

    def get_all_gem_forge_system_configs(self):
        return self.gem_forge_system_configs
