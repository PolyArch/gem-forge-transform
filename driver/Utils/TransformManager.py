import Constants as C

import os
import json


class TransformConfig(object):

    def __init__(self, fn):
        self.fn = fn
        with open(fn, 'r') as f:
            self.json = json.load(f)
            assert('id' in self.json)
            assert('options' in self.json)

    def get_transform_id(self):
        return self.json['id']

    def get_options(self, benchmark, trace):
        # Simple hacky preprocessing.
        options = list()
        trace_id = trace.get_trace_id()
        for o in self.json['options']:
            if '{' in o:
                options.append(
                    o.format(
                        RUN_PATH=benchmark.get_run_path(),
                        BENCHMARK=benchmark.get_name(),
                        TRACE_ID=trace_id))
            else:
                options.append(o)
        return options

    def get_transform(self):
        return self.json['transform']

    def get_debugs(self):
        return []


class TransformManager(object):
    def __init__(self, transform_fns):
        self.configs = dict()
        self.transform_ids_in_order = list()
        for fn in transform_fns:
            config = TransformConfig(fn)
            self.configs[config.get_transform_id()] = config
            self.transform_ids_in_order.append(config.get_transform_id())

    def has_config(self, transform_id):
        return transform_id in self.configs

    def get_config(self, transform_id):
        return self.configs[transform_id]

    def get_all_configs(self):
        return [self.get_config(transform_id) for transform_id in self.transform_ids_in_order]

    def get_all_transform_ids(self):
        # Return them in order.
        return self.transform_ids_in_order
