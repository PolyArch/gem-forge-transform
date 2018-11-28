import Constants as C
import os


class Gem5ReplayConfig(object):

    THROTTLING_DEFAULT = 'static'
    ENABLE_COALESCE_DEFAULT = 'single'
    L1_DCACHE_DEFAULT = 'original'
    L1D_MSHRS_DEFAULT = 4
    L2BUS_WIDTH_DEFAULT = 32

    def __init__(self, **kwargs):
        self.prefetch = kwargs['prefetch']
        self.stream_engine_prefetch = kwargs['stream_engine_prefetch']
        self.l1d_mshrs = kwargs['l1d_mshrs']
        self.l1d_assoc = kwargs['l1d_assoc']
        self.l2bus_width = kwargs['l2bus_width']
        self.l1_5d = kwargs['l1_5d']
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

    def get_options(self, tdg):
        dirname, tdg_base = os.path.split(tdg)
        if '.replay.' in tdg_base:
            transform = 'replay'
        if '.stream.' in tdg_base:
            transform = 'stream'
        options = list()
        L2Size = '1MB'
        L2TagLat = 20
        if self.l1d_mshrs != Gem5ReplayConfig.L1D_MSHRS_DEFAULT:
            options.append(
                '--l1d_mshrs={l1d_mshrs}'.format(l1d_mshrs=self.l1d_mshrs)
            )
        options.append(
            '--l2bus_width={l2bus_width}'.format(l2bus_width=self.l2bus_width)
        )
        if self.l1_5d:
            options.append(
                '--l1_5dcache'
            )
            L2Size = '8MB'
        if transform == 'replay':
            if self.prefetch:
                options.append(
                    '--llvm-prefetch=1'
                )
        if transform == 'stream':
            if self.stream_engine_prefetch:
                options.append(
                    '--llvm-prefetch=1'
                )
            if self.stream_engine_is_oracle:
                options.append(
                    '--gem-forge-stream-engine-is-oracle=1'
                )
            if self.stream_engine_max_run_ahead_length is not None:
                options.append(
                    '--gem-forge-stream-engine-max-run-ahead-length={x}'.format(
                        x=self.stream_engine_max_run_ahead_length
                    )
                )
            if self.stream_engine_throttling is not None:
                options.append(
                    '--gem-forge-stream-engine-throttling={x}'.format(
                        x=self.stream_engine_throttling
                    )
                )
            if self.stream_engine_enable_coalesce == 'coalesce':
                options.append(
                    '--gem-forge-stream-engine-enable-coalesce=1'
                )
            elif self.stream_engine_enable_coalesce == 'merge':
                # Enable merge also enables coalesce.
                options.append(
                    '--gem-forge-stream-engine-enable-coalesce=1'
                )
                options.append(
                    '--gem-forge-stream-engine-enable-merge=1'
                )
            if self.stream_engine_l1d != 'original':
                options.append(
                    '--gem-forge-stream-engine-l1d={l1d}'.format(
                        l1d=self.stream_engine_l1d)
                )
        options.append(
            '--l2_size={l2size}'.format(l2size=L2Size),
        )
        options.append(
            '--l1d_assoc={l1d_assoc}'.format(l1d_assoc=self.l1d_assoc),
        )
        return options

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
        if self.l1d_mshrs != Gem5ReplayConfig.L1D_MSHRS_DEFAULT:
            config += '.l1dmshr{l1d_mshrs}'.format(l1d_mshrs=self.l1d_mshrs)
        if self.l1d_assoc != 2:
            config += '.l1dassoc{l1d_assoc}'.format(l1d_assoc=self.l1d_assoc)
        if self.l1_5d:
            config += '.l1_5d'
        if self.l2bus_width != Gem5ReplayConfig.L2BUS_WIDTH_DEFAULT:
            config += '.l2bw{l2bus_width}'.format(l2bus_width=self.l2bus_width)
        if transform == 'replay':
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
            if self.stream_engine_prefetch:
                config += '.se_prefetch'
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
                prefetch=self.options.replay_prefetch,
                stream_engine_prefetch=self.options.se_prefetch,
                l1d_mshrs=self.options.l1d_mshrs,
                l1d_assoc=self.options.l1d_assoc,
                l2bus_width=self.options.l2bus_width,
                l1_5d=self.options.l1_5d,
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
                        prefetch=self.options.replay_prefetch,
                        stream_engine_prefetch=self.options.se_prefetch,
                        l1d_mshrs=self.options.l1d_mshrs,
                        l1d_assoc=self.options.l1d_assoc,
                        l2bus_width=self.options.l2bus_width,
                        l1_5d=self.options.l1_5d,
                        stream_engine_is_oracle=self.options.se_oracle,
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
                    prefetch=self.options.replay_prefetch,
                    stream_engine_prefetch=self.options.se_prefetch,
                    l1d_mshrs=self.options.l1d_mshrs,
                    l1d_assoc=self.options.l1d_assoc,
                    l2bus_width=self.options.l2bus_width,
                    l1_5d=self.options.l1_5d,
                    stream_engine_is_oracle=self.options.se_oracle,
                    stream_engine_max_run_ahead_length=10,
                    stream_engine_throttling=self.options.se_throttling,
                    stream_engine_enable_coalesce=self.options.se_coalesce,
                    stream_engine_l1d=self.options.se_l1d,
                )
            )
