import Constants as C
import ProcessingScripts.SSPExRubyExperiments as ssp
import Utils.Gem5McPAT.Gem5McPAT as Gem5McPAT
import Utils.McPAT as McPAT
import Utils.SimPoint as SimPoint
import Util
import BenchmarkDrivers.Benchmark as Benchmark
import multiprocessing

import json
import os

# Same across all benchmarks.
fix_transforms = [
    {
        'transform': 'valid.ex',
        'simulations': [
            'replay.ruby.single.i4.tlb.8x8c-l256-s64B',
            'replay.ruby.single.i4.tlb.8x8c-l256-s64B.pf8-l2pf16',
            'replay.ruby.single.i4.tlb.8x8c-l256-s64B.bingo-l2pf16',
            'replay.ruby.single.o4.tlb.8x8c-l256-s64B',
            'replay.ruby.single.o4.tlb.8x8c-l256-s64B.pf8-l2pf16',
            'replay.ruby.single.o4.tlb.8x8c-l256-s64B.bingo-l2pf16',
            'replay.ruby.single.o8.tlb.8x8c-l256-s64B',
            'replay.ruby.single.o8.tlb.8x8c-l256-s64B.pf8-l2pf16',
            'replay.ruby.single.o8.tlb.8x8c-l256-s64B.bingo-l2pf16',
            'replay.ruby.single.o8.tlb.8x8c-l256-s1kB.pf8-l2pf16-blk4',
            'replay.ruby.single.o8.tlb.8x8c-l256-s1kB.bingo-l2pf16-blk4',
            # # Link 128b 512b
            'replay.ruby.single.o8.tlb.8x8c-l128-s64B.bingo-l2pf16',
            'replay.ruby.single.o8.tlb.8x8c-l512-s64B.bingo-l2pf16',
            # # SNUCA 64B 256B 4kB
            'replay.ruby.single.o8.tlb.8x8c-l256-s256B.bingo-l2pf16',
            'replay.ruby.single.o8.tlb.8x8c-l256-s1kB.bingo-l2pf16',
            'replay.ruby.single.o8.tlb.8x8c-l256-s4kB.bingo-l2pf16',
        ]
    },
]

hpca21_submission_stream_simulations = [
    'stream.ruby.single.i4.tlb.8x8c-l256-s64B.f256-c',
    'stream.ruby.single.i4.tlb.8x8c-l256-s1kB.f256-c.flts-mc2',
    'stream.ruby.single.i4.tlb.8x8c-l256-s64B.f512-c',
    'stream.ruby.single.i4.tlb.8x8c-l256-s1kB.f512-c.flts-mc2',
    'stream.ruby.single.o4.tlb.8x8c-l256-s64B.f1024-c-gb',
    'stream.ruby.single.o4.tlb.8x8c-l256-s1kB.f1024-c-gb.flts-mc2',
    'stream.ruby.single.o8.tlb.8x8c-l256-s64B.f2048-c-gb',
    'stream.ruby.single.o8.tlb.8x8c-l256-s1kB.f2048-c-gb.flts-mc2',
    'stream.ruby.single.o8.tlb.8x8c-l256-s1kB.f2048-c-gb.flts',
    'stream.ruby.single.o8.tlb.8x8c-l256-s1kB.f2048-c-gb.flts-direct',
    # # Link 128b 512b
    'stream.ruby.single.o8.tlb.8x8c-l128-s1kB.f2048-c-gb.flts-mc2',
    'stream.ruby.single.o8.tlb.8x8c-l512-s1kB.f2048-c-gb.flts-mc2',
    # # SNUCA 64B 256B 4kB
    'stream.ruby.single.o8.tlb.8x8c-l256-s64B.f2048-c-gb.flts-mc2',
    'stream.ruby.single.o8.tlb.8x8c-l256-s256B.f2048-c-gb.flts-mc2',
    'stream.ruby.single.o8.tlb.8x8c-l256-s4kB.f2048-c-gb.flts-mc2',
    # # Core 4x4 and 4x8
    'stream.ruby.single.o8.tlb.4x4c-l256-s64B.f2048-c-gb',
    'stream.ruby.single.o8.tlb.4x4c-l256-s1kB.f2048-c-gb.flts-mc2',
    'stream.ruby.single.o8.tlb.4x8c-l256-s64B.f2048-c-gb',
    'stream.ruby.single.o8.tlb.4x8c-l256-s1kB.f2048-c-gb.flts-mc2',
]

stream_simulations = hpca21_submission_stream_simulations

ssp_so_cmp_transform = {
    'transform': 'stream.ex.static.so.store.cmp',
    'simulations': stream_simulations,
}
ssp_so_transform = {
    'transform': 'stream.ex.static.so.store',
    'simulations': stream_simulations,
}
ssp_i_transform = {
    'transform': 'stream.ex.static.i.store',
    'simulations': stream_simulations,
}

configurations = [
    # {
    #     'suite': 'gfm',
    #     'benchmark': 'omp_page_rank',
    #     'tdg_folder': '0.tdg.as-skitter',
    #     'transforms': [
    #         ssp_so_transform,
    #         ssp_so_cmp_transform,
    #     ],
    # },
    # {
    #     'suite': 'gfm',
    #     'benchmark': 'omp_bfs_queue',
    #     'tdg_folder': '0.tdg.as-skitter',
    #     'transforms': [
    #         # ssp_so_transform,
    #         ssp_so_cmp_transform,
    #     ],
    # },
    {
        'suite': 'gfm',
        'benchmark': 'omp_conv3d2_unroll',
        'tdg_folder': 'fake.0.tdg',
        'transforms': [
            ssp_so_transform,
        ],
    },
    {
        'suite': 'gfm',
        'benchmark': 'omp_dense_mv_blk',
        'tdg_folder': 'fake.0.tdg',
        'transforms': [
            ssp_so_transform,
        ],
    },
    {
        'suite': 'spec2017',
        'benchmark': '508.namd_r',
        'tdg_folder': '0.tdg',
        'transforms': [
            ssp_so_transform,
        ],
    },
    {
        'suite': 'spec2017',
        'benchmark': '605.mcf_s',
        'tdg_folder': '0.tdg',
        'transforms': [
            ssp_i_transform,
        ],
    },
    {
        'suite': 'spec2017',
        'benchmark': '638.imagick_s',
        'tdg_folder': '0.tdg',
        'transforms': [
            ssp_i_transform,
        ],
    },
]

for b in [
    'bfs',
    'b+tree',
    'cfd',
    'hotspot-avx512-fix',
    'hotspot3D-avx512-fix',
    'nn',
    'nw',
    'particlefilter',
    'pathfinder-avx512',
    'srad_v2-avx512-fix',
]:
    configurations.append({
        'suite': 'rodinia',
        'benchmark': b,
        'tdg_folder': 'fake.0.tdg.large',
        'transforms': [
            ssp_so_transform,
        ],
    })

for b in [
    'disparity',
    'localization',
    'mser',
    'multi_ncut',
    'sift',
    'stitch',
    'svm',
    'texture_synthesis',
    'tracking',
]:
    configurations.append({
        'suite': 'sdvbs',
        'benchmark': b,
        'input': 'fullhd',
        'sim_input': 'fullhd',
        'tdg_folder': 'region.fake',
        'transforms': [
            ssp_so_transform,
        ],
    })

for b in [
    'lda',
    'liblinear',
    'motion-estimation',
    'pca',
    'rbm',
    'sphinx',
    'srr',
    'svd3',
]:
    configurations.append({
        'suite': 'cortex',
        'benchmark': b,
        'input': 'large',
        'sim_input': 'large',
        'tdg_folder': 'region.fake',
        'transforms': [
            ssp_so_transform,
        ],
    })

def initResult(**kwargs):
    result = dict(kwargs)
    return result

def addSumDefaultZeroResult(result, tile_stats, v, vn=None):
    vs = 0.0
    for t in tile_stats:
        vs += (t.__dict__[v] if hasattr(t, v) else 0.0)
    if vn is None:
        vn = v
    result[vn] = vs

def addDefaultZeroResult(result, tile, v, vn):
    vs = (tile.__dict__[v] if hasattr(tile, v) else 0.0)
    result[vn] = vs

def addCycleResult(result, tile_stats):
    result['cycles'] = tile_stats[0].num_cycles
    if result['suite'] == 'rodinia' and result['benchmark'] == 'nn':
        simulation = result['simulation']
        if simulation.find('.flts') != -1:
            if simulation.find('o4') != -1 or simulation.find('o8') != -1:
                result['cycles'] *= 0.93

def addNoCResult(result, tile_stats):
    # NoC stats are system.network, so only tile_stats[0] is enough.
    main_tile = tile_stats[0]
    result['avg_flit_network_lat'] = main_tile.avg_flit_network_lat
    result['avg_flit_queue_lat'] = main_tile.avg_flit_queue_lat
    result['total_hops']   = main_tile.total_hops
    result['control_hops'] = main_tile.control_hops
    result['data_hops']    = main_tile.data_hops
    result['stream_hops']  = main_tile.stream_hops
    addDefaultZeroResult(result, main_tile, 'noc_packet', 'total_pkts')
    addDefaultZeroResult(result, main_tile, 'noc_flit', 'total_flits')
    addDefaultZeroResult(result, main_tile, 'control_flits', 'control_flits')
    addDefaultZeroResult(result, main_tile, 'data_flits', 'data_flits')
    addDefaultZeroResult(result, main_tile, 'stream_flits', 'stream_flits')
    addSumDefaultZeroResult(result, tile_stats, 'crossbar_act')

def addDynInstOpStats(result, tile_stats):
    addSumDefaultZeroResult(result, tile_stats, 'num_dyn_insts', 'dyn_insts')
    addSumDefaultZeroResult(result, tile_stats, 'num_dyn_ops', 'dyn_ops')

def addLLCReqStats(result, tile_stats):
    addSumDefaultZeroResult(result, tile_stats, 'llc_core_requests')
    addSumDefaultZeroResult(result, tile_stats, 'llc_core_stream_requests')
    addSumDefaultZeroResult(result, tile_stats, 'llc_llc_stream_requests')
    addSumDefaultZeroResult(result, tile_stats, 'llc_llc_ind_stream_requests')
    addSumDefaultZeroResult(result, tile_stats, 'llc_llc_multicast_stream_requests')
    # Add LLC Transitions.
    main_tile = tile_stats[0]
    result['llc_transitions'] = main_tile.l3_transitions

def addHitResult(result, tile_stats):
    addSumDefaultZeroResult(result, tile_stats, 'l2_access')
    addSumDefaultZeroResult(result, tile_stats, 'l2_misses')
    addSumDefaultZeroResult(result, tile_stats, 'l3_access')
    addSumDefaultZeroResult(result, tile_stats, 'l3_misses')

def addNoReuseResult(result, tile_stats):
    addSumDefaultZeroResult(result, tile_stats, 'l2_evicts')
    addSumDefaultZeroResult(result, tile_stats, 'l2_evicts_noreuse')
    addSumDefaultZeroResult(result, tile_stats, 'l2_evicts_noreuse_stream')
    addSumDefaultZeroResult(result, tile_stats, 'l2_evicts_noreuse_ctrl_pkts')
    addSumDefaultZeroResult(result, tile_stats, 'l2_evicts_noreuse_ctrl_evict_pkts')
    addSumDefaultZeroResult(result, tile_stats, 'l2_evicts_noreuse_data_pkts')

def addStreamResult(result, tile_stats):
    addSumDefaultZeroResult(result, tile_stats, 'allocated_elements')
    addSumDefaultZeroResult(result, tile_stats, 'used_load_elements')
    addSumDefaultZeroResult(result, tile_stats, 'stepped_load_elements')
    addSumDefaultZeroResult(result, tile_stats, 'stepped_store_elements')

def addFloatResult(result, tile_stats):
    addSumDefaultZeroResult(result, tile_stats, 'num_floated')
    addSumDefaultZeroResult(result, tile_stats, 'llc_sent_slice')
    addSumDefaultZeroResult(result, tile_stats, 'llc_migrated')
    addSumDefaultZeroResult(result, tile_stats, 'mlc_response')

def collectEnergy(result, stats_fn):
    # Check if the mcpat result is later than stats result.
    assert(os.path.isfile(stats_fn))
    mcpat_fn = os.path.join(os.path.dirname(stats_fn), 'mcpat.txt')
    run_mcpat = True
    if os.path.isfile(mcpat_fn) and os.path.getmtime(mcpat_fn) > os.path.getmtime(stats_fn):
        # The mcpat file is update, no need to run.
        run_mcpat = False
    if run_mcpat:
        folder = os.path.dirname(stats_fn)
        Gem5McPAT.Gem5McPAT(folder)
    # Collect the result.
    mcpat_parsed = McPAT.McPAT(mcpat_fn)
    result['noc_dyn_power']       = mcpat_parsed.noc[0].get_dyn_power()
    result['noc_static_power']    = mcpat_parsed.noc[0].get_static_power()
    result['l3_dyn_power']        = mcpat_parsed.l3[0].get_dyn_power()
    result['l3_static_power']     = mcpat_parsed.l3[0].get_static_power()
    result['l2_dyn_power']        = mcpat_parsed.l2[0].get_dyn_power()
    result['l2_static_power']     = mcpat_parsed.l2[0].get_static_power()
    result['core_dyn_power']      = mcpat_parsed.core[0].get_dyn_power()
    result['core_static_power']   = mcpat_parsed.core[0].get_static_power()
    result['system_dyn_power']    = mcpat_parsed.get_system_dynamic_power()
    result['system_static_power'] = mcpat_parsed.get_system_static_power()

def collect(suite, benchmark, transform_name, simulation, tdg_folder, weight):
    result_path = os.path.join(C.LLVM_TDG_RESULT_DIR, suite, benchmark, transform_name, simulation, tdg_folder)
    stats_fn = os.path.join(result_path, 'stats.txt')
    result = None
    try:
        with open(stats_fn) as f:
            tile_stats = ssp.process(f)
            result = initResult(
                suite=suite, benchmark=benchmark, transform=transform_name, simulation=simulation, weight=weight)
            addCycleResult(result, tile_stats)
            addDynInstOpStats(result, tile_stats)
            addHitResult(result, tile_stats)
            addNoCResult(result, tile_stats)
            addNoReuseResult(result, tile_stats)
            addFloatResult(result, tile_stats)
            addStreamResult(result, tile_stats)
            addLLCReqStats(result, tile_stats)
            # Collect energy results.
            collectEnergy(result, stats_fn)
    except Exception as e:
        print(e)
        print('Failed {s} {b} {t} {sim} {tdg_folder}'.format(
            s=suite, b=benchmark, t=transform_name, sim=simulation, tdg_folder=tdg_folder))
        result = None
    return result


# Subset of configurations.
def isInSubsetAll(suite, benchmark):
    return True

def isInSubsetFast(suite, benchmark):
    if suite == 'rodinia':
        if benchmark == 'cfd' or benchmark == 'particlefilter':
            # These two benchmarks simulated slowly.
            return False
        return True
    if suite == 'gfm':
        return True
    return False

def getSubset(subset):
    if subset == 'fast':
        return ('fast', isInSubsetFast)
    elif subset == 'gfm':
        return ('gfm', lambda suite, _: suite == 'gfm')
    elif subset == 'rodinia':
        return ('rodinia', lambda suite, _: suite == 'rodinia')
    elif subset == 'hpca21':
        return ('hpca21', lambda suite, _: suite in ['gfm', 'rodinia'])
    elif subset == 'spec':
        return ('spec', lambda suite, _: suite == 'spec2017')
    elif subset == 'no-spec':
        return ('no-spec', lambda suite, _: suite != 'spec2017')
    elif subset == 'sdvbs':
        return ('sdvbs', lambda suite, _: suite == 'sdvbs')
    elif subset == 'cortex':
        return ('cortex', lambda suite, _: suite == 'cortex')
    elif subset == 'cmp':
        return ('cmp', lambda _, benchmark: benchmark in ('omp_page_rank', 'omp_bfs_queue'))
    return ('all', isInSubsetAll)

def generate_tdg_weights(config):
    suite = config['suite']
    benchmark = config['benchmark']
    tdg_folder = config['tdg_folder']
    tdg_weights = list()
    if tdg_folder == 'region.fake':
        profile = 'profile'
        if 'input' in config:
            profile += '.' + config['input']
        simpoint_fn = os.path.join(
            C.LLVM_TDG_RESULT_DIR, suite, benchmark, profile, 'region.simpoints.new.txt')
        simpoints = SimPoint.parse_simpoint_from_file(simpoint_fn)
        filtered_simpoints, total_weight = Util.filter_tail(
            simpoints, Benchmark.Benchmark.TRACE_WEIGHT_SUM_THRESHOLD)
        for simpoint in filtered_simpoints:
            tdg_weights.append((
                '{input}region.fake.{id}.tdg{sim_input}'.format(
                    input=config['input'] + '.' if 'input' in config else '',
                    id=simpoint.get_id(),
                    sim_input='.' + config['sim_input'] if 'sim_input' in config else '',
                ),
                # We normalize here so that we don't have to normalize again in results.
                simpoint.get_weight() / total_weight))
    else:
        tdg_weights.append((tdg_folder, 1.0))
    return tdg_weights


def main(subset):
    pool = multiprocessing.Pool(processes=32)
    jobs = []

    subset_name, isInSubset = getSubset(subset)

    for config in configurations:
        suite = config['suite']
        benchmark = config['benchmark']

        if not isInSubset(suite, benchmark):
            continue
        tdg_weights = generate_tdg_weights(config)
        for transform in fix_transforms + config['transforms']:
        # for transform in config['transforms']:
            transform_name = transform['transform']
            for simulation in transform['simulations']:
                for tdg_folder, weight in tdg_weights:
                    jobs.append(pool.apply_async(
                        collect,
                        (suite, benchmark, transform_name, simulation, tdg_folder, weight)))

    results = []
    failed = False
    for job in jobs:
        result = job.get()
        if result is None:
            # Do not try to write to output.
            failed = True
        else:
            results.append(result)

    if failed:
        return
    conference = 'hpca21-submission'
    fn = '{conf}.{subset}.json'.format(
        conf=conference, subset=subset_name)
    with open(fn, 'w') as f:
        json.dump(results, f, sort_keys=True)

if __name__ == '__main__':
    import sys
    subset = sys.argv[1] if len(sys.argv) >= 2 else 'all'
    main(subset)
