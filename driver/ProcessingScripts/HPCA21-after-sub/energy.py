import json

with open('micro.rodinia.stream.json') as f:
    results = json.load(f)

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

def computeAverage(vs):
    return sum(vs) / len(vs)

def computeAdjustNoCDynamicPower(v):
    # The NoC power in McPAT is too high.
    # We divide NoC power by 8.
    noc_dyn_power = v['noc_dyn_power']
    return noc_dyn_power * 0.125

def computeAdjustDynamicPower(v):
    system_dyn_power = v['system_dyn_power']
    noc_dyn_power = v['noc_dyn_power']
    return system_dyn_power - noc_dyn_power + computeAdjustNoCDynamicPower(v)

def computeAdjustStaticPower(vs):
    # The static power in McPAT is not making sense at all.
    # Use 15% of the highest dynamic power.
    highest_dynamic_power = max([computeAdjustDynamicPower(x) for x in vs]) 
    return highest_dynamic_power * 0.15

def computeEnergy(baseline_trans, baseline_sim, trans, sim):
    print(f'Energy Efficiency over baseline {baseline_trans} {baseline_sim}')
    baselines = [x for x in results if matchTransSim(x, baseline_trans, baseline_sim)]
    configs = [x for x in results if matchTransSim(x, trans, sim)]

    base_static_power = computeAdjustStaticPower(baselines)
    config_static_power = computeAdjustStaticPower(configs)

    energy_efficiency = []
    noc_energy = []
    for x in configs:
        if matchTransSim(x, baseline_trans, baseline_sim):
            continue
        suite = x['suite']
        bench = x['benchmark']
        trans = x['transform']
        sim = x['simulation']
        baseline = next(b for b in baselines if matchSuiteBench(b, suite, bench))

        base_energy = (computeAdjustDynamicPower(baseline) + base_static_power) * baseline['cycles']
        config_energy = (computeAdjustDynamicPower(x) + config_static_power) * x['cycles']

        config_noc_energy = computeAdjustNoCDynamicPower(x) * x['cycles']
        config_dyn_energy = computeAdjustDynamicPower(x) * x['cycles']

        efficiency = base_energy / config_energy
        energy_efficiency.append(efficiency)

        noc_percentage = config_noc_energy / config_dyn_energy
        noc_energy.append(noc_percentage)
        print(f'{suite:10} {bench:20} {trans:30} {sim[19:]:32} {efficiency:.3} {noc_percentage:.2}') 
    avg_efficiency = computeAverage(energy_efficiency)
    print(f'Average Efficiency {avg_efficiency}')

    avg_noc_percentage = computeAverage(noc_energy)
    print(f'Average NoC Percentage {avg_noc_percentage}')


# computeSpeedup('valid.ex', 'replay.ruby.single.link16.i4.tlb.8x8c')
ruby = '8x8c'
link = 16
computeEnergy('stream.ex.static.', f'stream.ruby.single.link{link}.fifo48.i4.tlb.{ruby}.c',
              'stream.ex.static.', f'stream.ruby.single.link{link}.fifo48.i4.tlb.{ruby}.c.flts')
computeEnergy('stream.ex.static.', f'stream.ruby.single.link16.fifo48.i4.tlb.{ruby}.c',
              'stream.ex.static.', f'stream.ruby.single.link16.fifo48.i4.tlb.{ruby}.c.flts.mc0')
