import json

def matchTransSim(x, trans, sim):
    return x['transform'].startswith(trans) and x['simulation'] == sim

def matchSuiteBench(x, suite, bench):
    return x['suite'] == suite and x['benchmark'] == bench

def computeGeomean(speedups):
    geomean = 1.0
    for s in speedups:
        geomean = geomean * s
    geomean = geomean ** (1/len(speedups))
    return geomean

def computeSpeedup(results, baseline_trans, baseline_sim, trans, sim):
    print(f'Speedup over baseline {baseline_trans} {baseline_sim}')
    baselines = [x for x in results if matchTransSim(x, baseline_trans, baseline_sim)]
    configs = [x for x in results if matchTransSim(x, trans, sim)]
    speedups = []
    for x in configs:
        if matchTransSim(x, baseline_trans, baseline_sim):
            continue
        suite = x['suite']
        bench = x['benchmark']
        trans = x['transform']
        sim = x['simulation']
        baseline = next(b for b in baselines if matchSuiteBench(b, suite, bench))
        speedup = baseline['cycles'] / x['cycles']
        print(f'{suite:10} {bench:20} {trans:30} {sim[19:]:32} {speedup}')
        speedups.append(speedup)
    geomean = computeGeomean(speedups)
    print(f'Geomean {geomean}')


# computeSpeedup('valid.ex', 'replay.ruby.single.link16.i4.tlb.8x8c')
def main(subset, ruby, link):
    with open(f'micro.rodinia.{subset}.json') as f:
        results = json.load(f)
    computeSpeedup(results,
        'stream.ex.static.', f'stream.ruby.single.link{link}.fifo48.i4.tlb.{ruby}.c',
        'stream.ex.static.', f'stream.ruby.single.link{link}.fifo48.i4.tlb.{ruby}.c.flts')
    computeSpeedup(results,
        'stream.ex.static.', f'stream.ruby.single.link{link}.fifo48.i4.tlb.{ruby}.c',
        'stream.ex.static.', f'stream.ruby.single.link{link}.fifo48.i4.tlb.{ruby}.c.flts.mc0')
    computeSpeedup(results,
        'stream.ex.static.', f'stream.ruby.single.link{link}.fifo48.i4.tlb.{ruby}.c',
        'stream.ex.static.', f'stream.ruby.single.link{link}.fifo48.i4.tlb.{ruby}.c.flts-c')

if __name__ == '__main__':
    import sys
    try:
        subset = sys.argv[1]
    except:
        subset = 'all'
    try:
        ruby = sys.argv[2]
    except:
        ruby = '8x8c'
    try:
        link = sys.argv[3]
    except:
        link = 32
    main(subset, ruby, link)

