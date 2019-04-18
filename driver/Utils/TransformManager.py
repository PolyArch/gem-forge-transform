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
        for fn in transform_fns:
            config = TransformConfig(fn)
            self.configs[config.get_transform_id()] = config

    def has_config(self, transform_id):
        return transform_id in self.configs

    def get_config(self, transform_id):
        return self.configs[transform_id]

    def get_all_configs(self):
        return self.configs.values()

    def get_transforms(self):
        return self.configs.keys()
