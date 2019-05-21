"""
This file analyzes the stream transform statistics.
Specifically, this will parse the StreamMessage.
"""

import Utils.StreamMessage_pb2 as StreamMessage_pb2

import json
import os


class StreamTransformStats(object):

    class RegionAnalyzer(object):
        def __init__(self, trace, fn):
            self.trace = trace
            self.streams = dict()
            stream_region = StreamMessage_pb2.StreamRegion()
            f = open(fn)
            stream_region.ParseFromString(f.read())
            f.close()
            for s in stream_region.streams:
                stream_name = s.name.split()[3]
                if stream_name not in self.streams:
                    self.streams[stream_name] = [s]
                else:
                    inserted = False
                    for i in range(len(self.streams[stream_name])):
                        if self.streams[stream_name][i].config_loop_level > s.config_loop_level:
                            continue
                        self.streams[stream_name].insert(i, s)
                        inserted = True
                        break
                    if not inserted:
                        self.streams[stream_name].append(s)

        def get_mismatch_streams_at_level(self, level):
            mismatch_streams = list()
            for stream_name in self.streams:
                streams = self.streams[stream_name]
                if len(streams) <= level:
                    continue
                s = streams[level]
                if s.dynamic_info.is_qualified != s.static_info.is_qualified:
                    mismatch_streams.append(s)
            return mismatch_streams

        def sum_mem_stream_at_inner_most(self, func):
            value = 0
            for stream_name in self.streams:
                s = self.streams[stream_name][0]
                if s.type == 'phi':
                    continue
                value += func(s)
            return value

        def sum_mem_stream_at_qualified_outer_most(self, func):
            value = 0
            for stream_name in self.streams:
                qualified = None
                for s in self.streams[stream_name]:
                    if s.type == 'phi':
                        continue
                    if s.dynamic_info.is_qualified:
                        qualified = s
                if qualified is not None:
                    value += func(s)
            return value

        def get_total_stream_accesses(self):
            def count_if_qualified(s):
                if not s.dynamic_info.is_qualified:
                    return 0
                return s.dynamic_info.total_accesses
            return self.sum_mem_stream_at_inner_most(count_if_qualified)

        def get_total_inline_loop_mem_accesses(self):
            def count_if_qualified(s):
                return s.dynamic_info.total_accesses
            return self.sum_mem_stream_at_inner_most(count_if_qualified)

        def get_total_stream_accesses_by_type(self, pattern):
            # ! Be careful about the pattern.
            def count_if_qualified(s):
                if not s.dynamic_info.is_qualified:
                    return 0
                if s.static_info.val_pattern != pattern:
                    return 0
                return s.dynamic_info.total_accesses
            return self.sum_mem_stream_at_inner_most(count_if_qualified)

        def get_stream_accesses_longer_than(self, length):
            def count_if_longer_than(s):
                total_accesses = s.dynamic_info.total_accesses
                total_configs = s.dynamic_info.total_configures
                avg_length = float(total_accesses) / float(total_configs)
                if avg_length <= length:
                    return 0
                return s.dynamic_info.total_accesses
            return self.sum_mem_stream_at_qualified_outer_most(count_if_longer_than)

        def get_stream_accesses_with_control(self, pattern):
            # * pattern:
            # * 0: No control. 1: CondStep. 2: CondUse. 3: CondBoth.
            def count_if_control(s):
                total_accesses = s.dynamic_info.total_accesses
                total_iters = s.dynamic_info.total_iters
                cond_use = float(total_accesses) < float(total_iters) * 0.99
                cond_step = \
                    s.static_info.stp_pattern == StreamMessage_pb2.StreamStepPattern.Value(
                        'CONDITIONAL')
                if pattern == 0 and (not cond_use) and (not cond_step):
                    return s.dynamic_info.total_accesses
                if pattern == 1 and (not cond_use) and cond_step:
                    return s.dynamic_info.total_accesses
                if pattern == 2 and cond_use and (not cond_step):
                    return s.dynamic_info.total_accesses
                if pattern == 3 and cond_use and cond_step:
                    return s.dynamic_info.total_accesses
                return 0
            return self.sum_mem_stream_at_qualified_outer_most(count_if_control)

        def get_total_qualified_mem_accesses(self):
            total = 0
            for stream_name in self.streams:
                s = self.streams[stream_name][0]
                if s.type == 'phi':
                    continue
                if not s.dynamic_info.is_qualified:
                    continue
                total += s.dynamic_info.total_accesses
            return total

        def get_total_aliased_loop_mem_accesses(self):
            total = 0
            for stream_name in self.streams:
                s = self.streams[stream_name][0]
                if s.type == 'phi':
                    continue
                if not s.dynamic_info.is_aliased:
                    continue
                total += s.dynamic_info.total_accesses
            return total

        def get_total_loop_mem_accesses(self):
            total = 0
            for stream_name in self.streams:
                s = self.streams[stream_name][0]
                if s.type == 'phi':
                    continue
                total += s.dynamic_info.total_accesses
            return total

        def get_chosen_streams(self):
            chosen_streams = list()
            for stream_name in self.streams:
                for s in self.streams[stream_name]:
                    if s.dynamic_info.is_chosen:
                        chosen_streams.append(s)
                        break
            return chosen_streams

    class TraceAnalyzer(object):
        def __init__(self, parent, trace):
            self.parent = parent
            self.trace = trace
            self.load_stream_regions()
            self.load_transform_stats()

        def load_stream_regions(self):
            # Load the stream regions.
            self.regions = list()
            tdg_extra_path = self.parent.benchmark.get_tdg_extra_path(
                self.parent.stream_transform_config, self.trace)
            for item in os.listdir(tdg_extra_path):
                if not os.path.isdir(os.path.join(tdg_extra_path, item)):
                    continue
                stream_region_path = os.path.join(
                    tdg_extra_path, item, 'streams.info')
                self.regions.append(StreamTransformStats.RegionAnalyzer(
                    self.trace, stream_region_path))

            self.total_qualified_accesses = float(sum(
                [r.get_total_qualified_mem_accesses() for r in self.regions]))
            self.total_loop_accesses = float(sum(
                [r.get_total_loop_mem_accesses() for r in self.regions]))
            self.total_alise_accesses = float(sum(
                [r.get_total_aliased_loop_mem_accesses() for r in self.regions]))

        def load_transform_stats(self):
            # Load the basic stats from the transformation.
            tdg_stats_fn = self.parent.benchmark.get_tdg(
                self.parent.stream_transform_config, self.trace) + '.stats.txt'
            with open(tdg_stats_fn) as f:
                count = 0
                for line in f:
                    count += 1
                    fields = line.split()
                    if count == 2:
                        self.total_insts = float(fields[1])
                    if count == 3:
                        self.total_mem_accesses = float(fields[1])
                    if count == 4:
                        self.total_step_insts = float(fields[1])
                    if count == 5:
                        self.total_removed_insts = float(fields[1])
                    if count == 6:
                        self.total_config_insts = float(fields[1])

        def get_total_insts(self):
            return self.total_insts

        def get_total_removed_insts(self):
            return self.total_removed_insts

        def get_total_added_insts(self):
            return self.total_step_insts + self.total_config_insts * 2

        def get_total_mem_accesses(self):
            return self.total_mem_accesses

        def get_total_inline_loop_mem_accesses(self):
            return float(sum([
                r.get_total_inline_loop_mem_accesses() for r in self.regions]))

        def get_total_stream_accesses(self):
            return float(sum([
                r.get_total_stream_accesses() for r in self.regions]))

        def get_total_stream_accesses_by_type(self, pattern):
            return float(sum([
                r.get_total_stream_accesses_by_type(pattern) for r in self.regions]))

        def get_stream_accesses_longer_than(self, length):
            return float(sum([
                r.get_stream_accesses_longer_than(length) for r in self.regions]))

        def get_stream_accesses_with_control(self, pattern):
            return float(sum([
                r.get_stream_accesses_with_control(pattern) for r in self.regions]))

        def analyze_mismatch_at_level(self, level):
            total_mismatch_mem_accesses = 0
            for r in self.regions:
                mismatch_streams = r.get_mismatch_streams_at_level(level)
                for s in mismatch_streams:
                    dynamic_info = s.dynamic_info
                    static_info = s.static_info
                    if dynamic_info.is_aliased:
                        # Ignore the mismatch due to aliasing.
                        continue
                    if dynamic_info.total_accesses == 0:
                        # Ignore the mismatch due to no accesses in the trace.
                        continue
                    if not dynamic_info.is_qualified:
                        # Ingore the case where static reports stream but dynamic doesn't
                        continue
                    self.print_mismatch(s)
                    if s.type != 'phi':
                        total_mismatch_mem_accesses += dynamic_info.total_accesses
            if self.total_qualified_accesses != 0:
                total_mismatch_mem_accesses /= self.total_qualified_accesses
            return total_mismatch_mem_accesses

        def analyze_chosen_stream_length(self):
            total_stream_accesses = 0.0
            total_stream_length = 0.0
            for r in self.regions:
                chosen_streams = r.get_chosen_streams()
                for s in chosen_streams:
                    if s.type == 'phi':
                        # Ignore IndVarStreams.
                        continue
                    dynamic_info = s.dynamic_info
                    average_length = dynamic_info.total_accesses / dynamic_info.total_configures
                    total_stream_accesses += dynamic_info.total_accesses
                    total_stream_length += dynamic_info.total_accesses * average_length
            if total_stream_accesses != 0:
                return total_stream_length / total_stream_accesses
            else:
                return total_stream_length

        def print_mismatch(self, s):
            print('Mismatch stream! weight {weight:.2f} lv {lv:1} dynamic {dynamic:1} static {static:1} reason {reason:20}: {s}'.format(
                weight=(s.dynamic_info.total_accesses /
                        self.total_qualified_accesses *
                        self.trace.get_weight()) if self.total_qualified_accesses != 0 else 0.0,
                lv=s.loop_level - s.config_loop_level,
                dynamic=s.dynamic_info.is_qualified,
                static=s.static_info.is_qualified,
                reason=StreamMessage_pb2.StaticStreamInfo.StaticNotStreamReason.Name(
                    s.static_info.not_stream_reason),
                s=s.name
            ))

    def __init__(self, benchmark, stream_transform_config):
        self.benchmark = benchmark
        self.stream_transform_config = stream_transform_config
        self.analyze()

    def analyze(self):
        self.traces = list()
        self.mismatches = list()
        for trace in self.benchmark.get_traces():
            self.traces.append(StreamTransformStats.TraceAnalyzer(self, trace))

    def analyze_chosen_stream_length(self):
        average_stream_length = 0.0
        for trace in self.traces:
            average_stream_length += trace.analyze_chosen_stream_length() * \
                trace.trace.get_weight()
        return average_stream_length

    def analyze_aliase_stream_access(self):
        total_aliased_accesses = 0.0
        total_loop_accesses = 0.0
        for trace in self.traces:
            total_aliased_accesses += trace.total_alise_accesses * trace.trace.get_weight()
            total_loop_accesses += trace.total_loop_accesses
        return total_aliased_accesses / total_loop_accesses

    def weighted_by_trace(self, func):
        value = 0.0
        for trace in self.traces:
            value += func(trace) * trace.trace.get_weight()
        return value

    def get_total_insts(self):
        return self.weighted_by_trace(lambda trace: trace.get_total_insts())

    def get_total_added_insts(self):
        return self.weighted_by_trace(lambda trace: trace.get_total_added_insts())

    def get_total_removed_insts(self):
        return self.weighted_by_trace(lambda trace: trace.get_total_removed_insts())

    def get_total_mem_accesses(self):
        return self.weighted_by_trace(
            lambda trace: trace.get_total_mem_accesses())

    def get_total_inline_loop_mem_accesses(self):
        return self.weighted_by_trace(
            lambda trace: trace.get_total_inline_loop_mem_accesses())

    def get_total_stream_accesses(self):
        return self.weighted_by_trace(
            lambda trace: trace.get_total_stream_accesses())

    def get_total_stream_accesses_by_type(self, pattern):
        return self.weighted_by_trace(
            lambda trace: trace.get_total_stream_accesses_by_type(pattern))

    def get_stream_accesses_longer_than(self, length):
        return self.weighted_by_trace(
            lambda trace: trace.get_stream_accesses_longer_than(length))

    def get_stream_accesses_with_control(self, pattern):
        return self.weighted_by_trace(
            lambda trace: trace.get_stream_accesses_with_control(pattern))
