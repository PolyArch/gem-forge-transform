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

    def get_options(self):
        return self.json['options']

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

    def get_config(self, transform_id):
        return self.configs[transform_id]

    def get_all_configs(self):
        return self.configs.values()

    def get_transforms(self):
        return self.configs.keys()
