import Constants as C
import os


class Gem5ReplayConfig(object):

    THROTTLING_DEFAULT = 'static'
    ENABLE_COALESCE_DEFAULT = 'single'
    L1_DCACHE_DEFAULT = 'original'

    def __init__(self, **kwargs):
        self.prefetch = kwargs['prefetch']
        self.stream_engine_is_oracle = kwargs['stream_engine_is_oracle']
        if 'stream_engine_max_run_ahead_length' in kwargs:
            self.stream_engine_max_run_ahead_length = kwargs[
                'stream_engine_max_run_ahead_length'
            ]
        else:
            self.stream_engine_max_run_ahead_length = None
        if 'stream_engine_throttling' in kwargs:
            self.stream_engine_throttling = kwargs['stream_engine_throttling']
        else:
            self.stream_engine_throttling = Gem5ReplayConfig.THROTTLING_DEFAULT

        if 'stream_engine_enable_coalesce' in kwargs:
            self.stream_engine_enable_coalesce = kwargs['stream_engine_enable_coalesce']
        else:
            self.stream_engine_enable_coalesce = Gem5ReplayConfig.ENABLE_COALESCE_DEFAULT

        if 'stream_engine_l1d' in kwargs:
            self.stream_engine_l1d = kwargs['stream_engine_l1d']
        else:
            self.stream_engine_l1d = Gem5ReplayConfig.L1_DCACHE_DEFAULT

    def get_gem5_dir(self, tdg):
        dirname, basename = os.path.split(tdg)
        gem5_dir = os.path.join(dirname, self.get_config(basename))
        return gem5_dir

    def get_result(self, tdg):
        dirname, basename = os.path.split(tdg)
        return os.path.join(
            C.LLVM_TDG_RESULT_DIR,
            '{config}.txt'.format(
                config=self.get_config(basename),
            ))

    def get_config(self, tdg_basename):
        if '.stream.' in tdg_basename:
            return self.get_config_id('stream', tdg_basename)
        elif '.replay.' in tdg_basename:
            return self.get_config_id('replay', tdg_basename)
        else:
            print("Unable to extract transform from tdg {tdg}".format(
                tdg=tdg_basename
            ))
            raise ValueError(
                'Unable to extract transform pass from tdg file name.')

    def get_config_id(self, transform, prefix='config'):
        config = prefix
        config += '.{cpu_type}'.format(cpu_type=C.CPU_TYPE)
        if self.prefetch:
            config += '.prefetch'
        if transform == 'stream':
            if self.stream_engine_is_oracle:
                config += '.oracle'
            if self.stream_engine_max_run_ahead_length is not None:
                config += '.ahead{x}'.format(
                    x=self.stream_engine_max_run_ahead_length)
            if self.stream_engine_throttling != Gem5ReplayConfig.THROTTLING_DEFAULT:
                config += '.{throttling}'.format(
                    throttling=self.stream_engine_throttling
                )
            if self.stream_engine_enable_coalesce != Gem5ReplayConfig.ENABLE_COALESCE_DEFAULT:
                config += '.{enable_coalesce}'.format(
                    enable_coalesce=self.stream_engine_enable_coalesce
                )
            if self.stream_engine_l1d != Gem5ReplayConfig.L1_DCACHE_DEFAULT:
                config += '.{l1d}'.format(l1d=self.stream_engine_l1d)
        return config


class Gem5ReplayConfigureManager(object):
    def __init__(self, options):
        self.options = options
        self.configs = dict()
        self._init_replay_configs()
        self._init_stream_configs()

    def get_configs(self, transform_pass):
        return self.configs[transform_pass]

    def _init_replay_configs(self):
        self.configs['replay'] = [
            Gem5ReplayConfig(
                prefetch=False,
                stream_engine_is_oracle=False,
                stream_engine_max_run_ahead_length=2,
            ),
        ]

    def _init_stream_configs(self):
        self.configs['stream'] = list()
        if self.options.se_ahead is not None:
            for run_ahead_length in self.options.se_ahead:
                self.configs['stream'].append(
                    Gem5ReplayConfig(
                        prefetch=False,
                        stream_engine_is_oracle=False,
                        stream_engine_max_run_ahead_length=run_ahead_length,
                        stream_engine_throttling=self.options.se_throttling,
                        stream_engine_enable_coalesce=self.options.se_coalesce,
                        stream_engine_l1d=self.options.se_l1d,
                    )
                )
        else:
            # Default run ahead by 10
            self.configs['stream'].append(
                Gem5ReplayConfig(
                    prefetch=False,
                    stream_engine_is_oracle=False,
                    stream_engine_max_run_ahead_length=10,
                    stream_engine_throttling=self.options.se_throttling,
                    stream_engine_enable_coalesce=self.options.se_coalesce,
                    stream_engine_l1d=self.options.se_l1d,
                )
            )
