import Constants as C
import os


class TransformConfig(object):

    def get_id(self):
        pass

    def get_options(self):
        pass

    def get_transform(self):
        pass


class ReplayTransformConfig(TransformConfig):
    def __init__(self):
        pass

    def get_id(self):
        return 'replay'

    def get_options(self):
        return ['-replay']

    def get_transform(self):
        return 'replay'


class StreamTransformConfig(TransformConfig):
    def __init__(self, options):
        self.choose_strategy = options.stream_choose_strategy
        return

    def get_id(self):
        if self.choose_strategy == 'inner':
            return 'stream.in'
        return 'stream'

    def get_options(self):
        if self.choose_strategy == 'inner':
            return [
                '-stream-pass',
                '-stream-pass-choose-strategy=inner',
            ]
        return ['-stream-pass']

    def get_transform(self):
        return 'stream'

class StreamPrefetchTransformConfig(TransformConfig):
    def __init__(self, options):
        self.choose_strategy = options.stream_choose_strategy
        return

    def get_id(self):
        if self.choose_strategy == 'inner':
            return 'stream-prefetch.in'
        return 'stream-prefetch'

    def get_options(self):
        if self.choose_strategy == 'inner':
            return [
                '-stream-prefetch-pass',
                '-stream-pass-choose-strategy=inner',
            ]
        return ['-stream-prefetch-pass']

    def get_transform(self):
        return 'stream-prefetch'

class TransformManager(object):
    def __init__(self, options):
        self.options = options
        self.configs = dict()
        self._init_replay_transform()
        self._init_stream_transform()
        self._init_stream_prefetch_transform()

    def _init_replay_transform(self):
        if 'replay' not in self.options.transform_passes:
            return
        self.configs['replay'] = [ReplayTransformConfig()]

    def _init_stream_transform(self):
        if 'stream' not in self.options.transform_passes:
            return
        self.configs['stream'] = [StreamTransformConfig(self.options)]

    def _init_stream_prefetch_transform(self):
        if 'stream-prefetch' not in self.options.transform_passes:
            return
        self.configs['stream-prefetch'] = [StreamPrefetchTransformConfig(self.options)]

    def get_configs(self, transform):
        return self.configs[transform]
