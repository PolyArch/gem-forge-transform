"""
Simple process script to process stats for multi-core configurations.
Assume MinorCPU.
"""

import re


class TileStats(object):
    def __init__(self, tile_id):
        self.tile_id = tile_id
        self.load_blocked_cpi = 0
        self.load_blocked_ratio = 0
        self.l1d_access = 0
        self.l1d_misses = 0
        self.l2_access = 0
        self.l2_misses = 0
        self.l3_access = 0
        self.l3_misses = 0
        self.control_flits = 0
        self.data_flits = 0
        self.stream_flits = 0
        self.control_hops = 0
        self.data_hops = 0
        self.stream_hops = 0
        self.l3_transitions = dict()
        pass


class TileStatsParser(object):
    def __init__(self, tile_stats):
        self.tile_stats = tile_stats
        self.re = {
            'num_cycles': self.format_re(
                'system.future_cpus{tile_id}.numCycles'),
            'num_dyn_ops': self.format_re(
                'system.future_cpus{tile_id}.committedOps'),
            'num_dyn_insts': self.format_re(
                'system.future_cpus{tile_id}.committedInsts'),
            'load_blocked_cpi': self.format_re(
                'system.future_cpus{tile_id}.loadBlockedCPI'),
            'load_blocked_ratio': self.format_re(
                'system.future_cpus{tile_id}.loadBlockedCyclesPercen*'),
            'l1d_access': self.format_re(
                'system.ruby.l0_cntrl{tile_id}.Dcache.demand_accesses'),
            'l1d_misses': self.format_re(
                'system.ruby.l0_cntrl{tile_id}.Dcache.demand_misses'),
            'l2_access': self.format_re(
                'system.ruby.l1_cntrl{tile_id}.cache.demand_accesses'),
            'l2_misses': self.format_re(
                'system.ruby.l1_cntrl{tile_id}.cache.demand_misses'),
            'l2_evicts': self.format_re(
                'system.ruby.l1_cntrl{tile_id}.cache.deallocated'),
            'l2_evicts_noreuse': self.format_re(
                'system.ruby.l1_cntrl{tile_id}.cache.deallocated_no_reuse'),
            'l2_evicts_noreuse_stream': self.format_re(
                'system.ruby.l1_cntrl{tile_id}.cache.deallocated_no_reuse_stream'),
            'l2_evicts_noreuse_ctrl_pkts': self.format_re(
                'system.ruby.l1_cntrl{tile_id}.cache.deallocated_no_reuse_noc_ctrl_msg'),
            'l2_evicts_noreuse_ctrl_evict_pkts': self.format_re(
                'system.ruby.l1_cntrl{tile_id}.cache.deallocated_no_reuse_noc_ctrl_evict_msg'),
            'l2_evicts_noreuse_data_pkts': self.format_re(
                'system.ruby.l1_cntrl{tile_id}.cache.deallocated_no_reuse_noc_data_msg'),
            'l3_access': self.format_re(
                'system.ruby.l2_cntrl{tile_id}.L2cache.demand_accesses'),
            'l3_misses': self.format_re(
                'system.ruby.l2_cntrl{tile_id}.L2cache.demand_misses'),
            'l1tlb_access': self.format_re(
                'system.future_cpus{tile_id}.dtb.l1Accesses'),
            'l1tlb_misses': self.format_re(
                'system.future_cpus{tile_id}.dtb.l1Misses'),
            'l2tlb_access': self.format_re(
                'system.future_cpus{tile_id}.dtb.l2Accesses'),
            'l2tlb_misses': self.format_re(
                'system.future_cpus{tile_id}.dtb.l2Misses'),
            'noc_flit': ['system.ruby.network.flits_injected::total'],
            'noc_packet': ['system.ruby.network.packets_injected::total'],
            'avg_flit_network_lat': ['system.ruby.network.average_flit_network_latency'],
            'avg_flit_queue_lat': ['system.ruby.network.average_flit_queueing_latency'],
            'avg_sequencer_miss_lat': ['system.ruby.miss_latency_hist_seqr::mean'],
            'avg_sequencer_hit_lat': ['system.ruby.hit_latency_hist_seqr::mean'],
            'total_hops': ['system.ruby.network.total_hops'],
            'crossbar_act': self.format_re(
                'system.ruby.network.routers{tile_id}.crossbar_activity'),
            'commit_op': self.format_re(
                'system.future_cpus{tile_id}.committedOps'),
            'commit_inst': self.format_re(
                'system.future_cpus{tile_id}.committedInsts'),
            'idea_cycles': self.format_re(
                'system.future_cpus{tile_id}.ideaCycles$'),
            'idea_cycles_no_fu': self.format_re(
                'system.future_cpus{tile_id}.ideaCyclesNoFUTiming'),
            'idea_cycles_no_ld': self.format_re(
                'system.future_cpus{tile_id}.ideaCyclesNoLDTiming'),
            'allocated_elements': self.format_re(
                'system.future_cpus{tile_id}.accelManager.se.numElementsAllocated'),
            'used_load_elements': self.format_re(
                'system.future_cpus{tile_id}.accelManager.se.numLoadElementsUsed'),
            'stepped_load_elements': self.format_re(
                'system.future_cpus{tile_id}.accelManager.se.numLoadElementsStepped'),
            'stepped_store_elements': self.format_re(
                'system.future_cpus{tile_id}.accelManager.se.numStoreElementsStepped'),
            'num_floated': self.format_re(
                'system.future_cpus{tile_id}.accelManager.se.numFloated'),
            'llc_sent_slice': self.format_re(
                'system.future_cpus{tile_id}.accelManager.se.numLLCSentSlice'),
            'llc_migrated': self.format_re(
                'system.future_cpus{tile_id}.accelManager.se.numLLCMigrated'),
            'mlc_response': self.format_re(
                'system.future_cpus{tile_id}.accelManager.se.numMLCResponse'),
            'dcache_core_requests': self.format_re(
                'system.ruby.l0_cntrl{tile_id}.coreRequests'),
            'dcache_core_stream_requests': self.format_re(
                'system.ruby.l0_cntrl{tile_id}.coreStreamRequests'),
            'llc_core_requests': self.format_re(
                'system.ruby.l2_cntrl{tile_id}.coreRequests'),
            'llc_core_stream_requests': self.format_re(
                'system.ruby.l2_cntrl{tile_id}.coreStreamRequests'),
            'llc_llc_stream_requests': self.format_re(
                'system.ruby.l2_cntrl{tile_id}.llcStreamRequests'),
            'llc_llc_ind_stream_requests': self.format_re(
                'system.ruby.l2_cntrl{tile_id}.llcIndStreamRequests'),
            'llc_llc_multicast_stream_requests': self.format_re(
                'system.ruby.l2_cntrl{tile_id}.llcMulticastStreamRequests'),
            'stream_wait_cycles': self.format_re(
                'system.cpu{tile_id}.accelManager.se.numLoadElementWaitCycles'),
        }
        self.l2_transition_re = re.compile('system\.ruby\.L2Cache_Controller\.[A-Z_]+\.[A-Z0-9_]+::total')

    def format_re(self, expression):
        # Return two possible cases.
        return [
            expression.format(tile_id='{x}'.format(x=self.tile_stats.tile_id)),
            expression.format(tile_id='0{x}'.format(x=self.tile_stats.tile_id)),
        ]

    def parse(self, fields):
        if len(fields) < 2:
            return
        for k in self.re:
            for s in self.re[k]:
                if fields[0] == s:
                    self.tile_stats.__dict__[k] = float(fields[1])
                    break
        if fields[0] == 'system.ruby.network.flit_types_injected':
            ctrl, data, strm = self.parse_flit_type_breakdown(fields)
            self.tile_stats.control_flits += ctrl
            self.tile_stats.data_flits += data
            self.tile_stats.stream_flits += strm
        if fields[0] == 'system.ruby.network.total_hop_types':
            ctrl, data, strm = self.parse_flit_type_breakdown(fields)
            self.tile_stats.control_hops += ctrl
            self.tile_stats.data_hops += data
            self.tile_stats.stream_hops += strm
        self.parse_l2_stream_transition(fields)

    def parse_flit_type_breakdown(self, fields):
        # ! Keep this sync with CoherenceRequestType/CoherenceResponseType
        msg_type_category = [
            ('ctrl', 'Request::GETX'),
            ('ctrl', 'Request::UPGRADE'),
            ('ctrl', 'Request::GETS'),
            ('ctrl', 'Request::GETU'),
            ('ctrl', 'Request::GETH'),
            ('ctrl', 'Request::GET_INSTR'),
            ('ctrl', 'Request::INV'),
            ('data', 'Request::PUTX'),
            ('ctrl', 'Request::WB_ACK'),
            ('ctrl', 'Request::DMA_READ'),
            ('data', 'Request::DMA_WRITE'),
            ('strm', 'Request::STREAM_CONFIG'),
            ('strm', 'Request::STREAM_FLOW'),
            ('strm', 'Request::STREAM_END'),
            ('ctrl', 'Request::STREAM_STORE'),
            ('strm', 'Request::STREAM_MIGRATE'),
            ('ctrl', 'Response::MEMORY_ACK'),
            ('data', 'Response::DATA'),
            ('data', 'Response::DATA_EXCLUSIVE'),
            ('data', 'Response::MEMORY_DATA'),
            ('ctrl', 'Response::ACK'),
            ('ctrl', 'Response::WB_ACK'),
            ('ctrl', 'Response::UNBLOCK'),
            ('ctrl', 'Response::EXCLUSIVE_UNBLOCK'),
            ('ctrl', 'Response::INV'),
            ('strm', 'Response::STREAM_ACK'),
        ]
        n_types = 32
        flits = [float(fields[i * 4 + 2]) for i in range(n_types)]
        control_flits = 0
        data_flits = 0
        stream_flits = 0
        for i in range(len(msg_type_category)):
            msg_type = msg_type_category[i][0]
            # print('Flits {t} {f}'.format(t=msg_type_category[i][1], f=flits[i]))
            if msg_type == 'ctrl':
                control_flits += flits[i]
            elif msg_type == 'data':
                data_flits += flits[i]
            elif msg_type == 'strm':
                stream_flits += flits[i]
        return (control_flits, data_flits, stream_flits)

    def parse_l2_stream_transition(self, fields):
        match_obj = self.l2_transition_re.match(fields[0])
        if match_obj:
            vs = fields[0].split('.')
            state = vs[3]
            event = vs[4][:-7]
            if state not in self.tile_stats.l3_transitions:
                self.tile_stats.l3_transitions[state] = dict()
            self.tile_stats.l3_transitions[state][event] = float(fields[1])

def findTileIdForPrefix(x, prefix):
    if x.startswith(prefix):
        idx = x.find('.', len(prefix))
        if idx != -1:
            return int(x[len(prefix):idx])
    return -1

def findTileId(x):
    prefix = [
        'system.future_cpus',
        'system.ruby.l0_cntrl',
        'system.ruby.l1_cntrl',
        'system.ruby.l2_cntrl',
    ]
    for p in prefix:
        tileId = findTileIdForPrefix(x, p)
        if tileId != -1:
            return tileId
    return -1

def process(f):
    tile_stats = [TileStats(i) for i in range(64)]
    tile_stats_parser = [TileStatsParser(ts) for ts in tile_stats]
    for line in f:
        if line.find('End Simulation Statistics') != -1:
            break
        fields = line.split()
        if not fields:
            continue
        tile_id = findTileId(fields[0])
        if tile_id != -1:
            tile_stats_parser[tile_id].parse(fields)
            continue
        for p in tile_stats_parser:
            p.parse(fields)
    return tile_stats

def print_stats(tile_stats):
    def sum_or_nan(vs):
        s = sum(vs)
        if s == 0:
            return float('NaN')
        return s

    def value_or_nan(x, v):
        return x.__dict__[v] if hasattr(x, v) else float('NaN')

    def value_or_zero(x, v):
        if isinstance(x, dict):
            return x[v] if v in x else 0.0
        return x.__dict__[v] if hasattr(x, v) else 0.0

    print('total l3 accesses       {v}'.format(
        v=sum_or_nan(ts.l3_access for ts in tile_stats)
    ))
    print('total l3 miss rate      {v}'.format(
        v=sum(ts.l3_misses for ts in tile_stats) /
        sum_or_nan(ts.l3_access for ts in tile_stats)
    ))
    print('total l2 miss rate      {v}'.format(
        v=sum(ts.l2_misses for ts in tile_stats) /
        sum_or_nan(ts.l2_access for ts in tile_stats)
    ))
    print('total l1d miss rate     {v}'.format(
        v=sum(ts.l1d_misses for ts in tile_stats) /
        sum_or_nan(ts.l1d_access for ts in tile_stats)
    ))
    # print('l2tlb miss / tlb access {v}'.format(
    #     v=sum_or_nan(value_or_nan(ts, 'l2tlb_misses') for ts in tile_stats) /
    #     sum_or_nan(value_or_nan(ts, 'l1tlb_access') for ts in tile_stats)
    # ))
    # print('l2tlb miss / l2 access  {v}'.format(
    #     v=sum_or_nan(value_or_nan(ts, 'l2tlb_misses') for ts in tile_stats) /
    #     sum_or_nan(value_or_nan(ts, 'l2tlb_access') for ts in tile_stats)
    # ))
    # print('l1tlb miss / tlb access {v}'.format(
    #     v=sum_or_nan(value_or_nan(ts, 'l1tlb_misses') for ts in tile_stats) /
    #     sum_or_nan(value_or_nan(ts, 'l1tlb_access') for ts in tile_stats)
    # ))
    print('total l2 evicts         {v}'.format(
        v=sum_or_nan(value_or_nan(ts, 'l2_evicts') for ts in tile_stats)
    ))
    # print('load blocked cpi        {v}'.format(
    #     v=sum(ts.load_blocked_cpi for ts in tile_stats) /
    #     len(tile_stats)
    # ))
    # print('load blocked percentage {v}'.format(
    #     v=sum(ts.load_blocked_ratio for ts in tile_stats) /
    #     len(tile_stats)
    # ))
    main_ts = tile_stats[0]
    print('total hops              {v}'.format(
        v=value_or_nan(main_ts, 'total_hops')
    ))
    print('noc flits               {v}'.format(
        v=value_or_nan(main_ts, 'noc_flit')
    ))
    print('noc packets             {v}'.format(
        v=value_or_nan(main_ts, 'noc_packet')
    ))
    print('control hops            {v}'.format(
        v=value_or_nan(main_ts, 'control_hops')
    ))
    print('data hops               {v}'.format(
        v=value_or_nan(main_ts, 'data_hops')
    ))
    print('stream hops             {v}'.format(
        v=value_or_nan(main_ts, 'stream_hops')
    ))
    print('crossbar activity       {v}'.format(
        v=sum(value_or_nan(ts, 'crossbar_act') for ts in tile_stats)
    ))
    print('crossbar / cycle        {v}'.format(
        v=sum(value_or_nan(ts, 'crossbar_act') for ts in tile_stats) / \
            (len(tile_stats) * main_ts.num_cycles)
    ))
    print('main cpu cycles         {v}'.format(
        v=main_ts.num_cycles
    ))
    print('main cpu opc            {v}'.format(
        v=main_ts.commit_op / main_ts.num_cycles
    ))
    # print('main cpu idea opc       {v}'.format(
    #     v=main_ts.commit_op / main_ts.idea_cycles if hasattr(main_ts, 'idea_cycles') else 0
    # ))
    # print('main cpu idea opc no ld {v}'.format(
    #     v=main_ts.commit_op / main_ts.idea_cycles_no_ld if hasattr(main_ts, 'idea_cycles_no_ld') else 0
    # ))
    # print('main cpu idea opc no fu {v}'.format(
    #     v=main_ts.commit_op / main_ts.idea_cycles_no_fu if hasattr(main_ts, 'idea_cycles_no_fu') else 0
    # ))
    # print('main load blocked cpi   {v}'.format(
    #     v=main_ts.load_blocked_cpi
    # ))
    # print('main blocked percentage {v}'.format(
    #     v=main_ts.load_blocked_ratio
    # ))
    print('num elements allocated  {v}'.format(
        v=sum(value_or_zero(ts, 'allocated_elements') for ts in tile_stats)
    ))
    print('num ld elements used    {v}'.format(
        v=sum(value_or_zero(ts, 'used_load_elements') for ts in tile_stats)
    ))
    print('num ld elements stepped {v}'.format(
        v=sum(value_or_zero(ts, 'stepped_load_elements') for ts in tile_stats)
    ))
    print('num st elements stepped {v}'.format(
        v=sum(value_or_zero(ts, 'stepped_store_elements') for ts in tile_stats)
    ))
    print('num float               {v}'.format(
        v=sum(value_or_zero(ts, 'num_floated') for ts in tile_stats)
    ))
    print('num llc sent slice      {v}'.format(
        v=sum(value_or_zero(ts, 'llc_sent_slice') for ts in tile_stats)
    ))
    print('num llc migated         {v}'.format(
        v=sum(value_or_zero(ts, 'llc_migrated') for ts in tile_stats)
    ))
    print('num mlc response        {v}'.format(
        v=sum(value_or_zero(ts, 'mlc_response') for ts in tile_stats)
    ))
    print('num d$ core req         {v}'.format(
        v=sum(value_or_zero(ts, 'dcache_core_requests') for ts in tile_stats)
    ))
    print('num d$ core stream req  {v}'.format(
        v=sum(value_or_zero(ts, 'dcache_core_stream_requests') for ts in tile_stats)
    ))
    print('num llc core req        {v}'.format(
        v=sum(value_or_zero(ts, 'llc_core_requests') for ts in tile_stats)
    ))
    print('num llc core stream req {v}'.format(
        v=sum(value_or_zero(ts, 'llc_core_stream_requests') for ts in tile_stats)
    ))
    print('num llc llc stream req  {v}'.format(
        v=sum(value_or_zero(ts, 'llc_llc_stream_requests') for ts in tile_stats)
    ))
    print('num llc llc ind req     {v}'.format(
        v=sum(value_or_zero(ts, 'llc_llc_ind_stream_requests') for ts in tile_stats)
    ))
    print('num llc llc multi req   {v}'.format(
        v=sum(value_or_zero(ts, 'llc_llc_multicast_stream_requests') for ts in tile_stats)
    ))
    print('num l2 noreuse ctrl pkt {v}'.format(
        v=sum(value_or_zero(ts, 'l2_evicts_noreuse_ctrl_pkts') for ts in tile_stats)
    ))
    print('num l2 noreuse evic pkt {v}'.format(
        v=sum(value_or_zero(ts, 'l2_evicts_noreuse_ctrl_evict_pkts') for ts in tile_stats)
    ))
    print('num l2 noreuse data pkt {v}'.format(
        v=sum(value_or_zero(ts, 'l2_evicts_noreuse_data_pkts') for ts in tile_stats)
    ))
    print('num llc llc multi req   {v}'.format(
        v=sum(value_or_zero(ts, 'llc_llc_multicast_stream_requests') for ts in tile_stats)
    ))
    print('num llc GETV event      {v}'.format(
        v=sum([value_or_zero(main_ts.l3_transitions[s], 'L1_GETV') for s in main_ts.l3_transitions])
    ))


if __name__ == '__main__':
    import sys
    with open(sys.argv[1]) as f:
        tile_stats = process(f)
        print_stats(tile_stats)
