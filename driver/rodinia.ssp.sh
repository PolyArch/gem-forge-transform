#!/bin/sh

# rm -f /tmp/job_scheduler.*

Suite=rodinia
# All
# SSP so.store
# Benchmark=rodinia.bfs,rodinia.hotspot-avx512-fix,rodinia.hotspot3D-avx512-fix,rodinia.particlefilter,rodinia.pathfinder-avx512,rodinia.srad_v2-avx512-fix
# SSP i.store
# Benchmark=rodinia.b+tree,rodinia.nn,rodinia.nw
# Float so.store + flt
# Benchmark=rodinia.hotspot-avx512-fix,rodinia.hotspot3D-avx512-fix
# Float so.store + fltm
# Benchmark=rodinia.pathfinder-avx512,rodinia.srad_v2-avx512-fix
# Float i.store + fltm
# Benchmark=rodinia.b+tree,rodinia.nn,rodinia.nw

# Each
# Benchmark=rodinia.b+tree
# Benchmark=rodinia.bfs
# Benchmark=rodinia.cfd
# Benchmark=rodinia.hotspot-avx512-fix
# Benchmark=rodinia.hotspot3D-avx512-fix
# Benchmark=rodinia.nn,rodinia.particlefilter
# Benchmark=rodinia.nw
# Benchmark=rodinia.particlefilter
# Benchmark=rodinia.pathfinder-avx512
Benchmark=rodinia.srad_v2-avx512-fix
# Benchmark=rodinia.hotspot-avx512-fix,rodinia.hotspot3D-avx512-fix,rodinia.pathfinder-avx512,rodinia.srad_v2-avx512-fix
SimInputSize=large
# SimInputSize=medium
# SimInputSize=test
StreamTransform=stream/ex/static/so.store
# Benchmark=rodinia.bfs,rodinia.cfd,rodinia.hotspot-avx512-fix,rodinia.hotspot3D-avx512-fix,rodinia.nw,rodinia.particlefilter,rodinia.pathfinder-avx512,rodinia.srad_v2-avx512-fix
# StreamTransform=stream/ex/static/i.store # For b+tree, nn, nw
# Benchmark=rodinia.b+tree,rodinia.nn
# Benchmark=rodinia.nn
 
python Driver.py --suite $Suite \
    -b $Benchmark \
    --build 
python Driver.py --suite $Suite \
    -b $Benchmark \
    --trace --fake-trace
python Driver.py --suite $Suite \
    -b $Benchmark \
    -t $StreamTransform \
    -d --transform-debug StreamExecutionTransformer

run_ssp () {
    local suite=$1
    local bench=$2
    local trans=$3
    local rubyc=$4
    local input=$5
    local threads=$6
    local parallel=$7
    local sim_prefix=stream/ruby/single/link16/fifo48/i4.tlb
    local sim_ssp=$sim_prefix.$rubyc.c
    local sim_flts=$sim_prefix.$rubyc.c.flts
    local sim_mc0=$sim_prefix.$rubyc.c.flts.mc0
    local all_sim=$sim_ssp,$sim_flts,$sim_mc0
    # local all_sim=$sim_flts
    python Driver.py --suite $suite \
        -b $bench \
        -t $trans \
        --sim-configs $all_sim \
        --sim-input $input \
        --input-threads $threads \
        -s -j $parallel &
}
RubyConfig=8x8c
Threads=64
Parallel=3
run_ssp $Suite $Benchmark $StreamTransform $RubyConfig $SimInputSize $Threads $Parallel
# RubyConfig=4x4c
# Threads=16
# Parallel=1
# run_ssp $Suite $Benchmark $StreamTransform $RubyConfig $SimInputSize $Threads $Parallel