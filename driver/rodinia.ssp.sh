#!/bin/sh

# rm -f /tmp/job_scheduler.*

SimInputSize=large
# SimInputSize=medium
# SimInputSize=test
StreamTransform=stream/ex/static/so
# All
# Benchmark=rodinia.b+tree,rodinia.bfs,rodinia.cfd,rodinia.hotspot-avx512-fix,rodinia.hotspot3D-avx512-fix,rodinia.nn,rodinia.nw,rodinia.particlefilter,rodinia.pathfinder-avx512,rodinia.srad_v2-avx512-fix,gfm.omp_dense_mv_blk,gfm.omp_conv3d2
# Fast
# Benchmark=rodinia.b+tree,rodinia.bfs,rodinia.hotspot-avx513-fix,rodinia.hotspot3D-avx512-fix,rodinia.nn,rodinia.nw,rodinia.pathfinder-avx512,rodinia.srad_v2-avx512-fix,gfm.omp_conv3d2,gfm.omp_dense_mv_blk
# Slow
# Benchmark=rodinia.cfd,rodinia.particlefilter
Benchmark=rodinia.bfs
 
# python Driver.py -b $Benchmark --build 
# python Driver.py -b $Benchmark --trace --fake-trace
# python Driver.py -b $Benchmark -t $StreamTransform -d --transform-debug StreamRegionAnalyzer

run_ssp () {
    local bench=$1
    local trans=$2
    local rubyc=$3
    local input=$4
    local threads=$5
    local parallel=$6
    local i4=stream/ruby/single/i4.tlb.$rubyc.c
    local o4=stream/ruby/single/o4.tlb.$rubyc.c
    local o8=stream/ruby/single/o8.tlb.$rubyc.c-gb-fifo
    local o8_link=stream/ruby/single/o8.tlb.$rubyc-link.c
    # local all_sim=${o8_link}-gb-fifo.flts-mc
    local all_sim=${o8}.flts-mc
    python Driver.py -b $bench -t $trans \
        --sim-configs $all_sim \
        --sim-input $input \
        --input-threads $threads \
        -s -j $parallel \
        --gem5-debug RubyStreamLife | tee bfs.log &
}
RubyConfig=4x4c
Threads=1
Parallel=12
run_ssp $Benchmark $StreamTransform $RubyConfig $SimInputSize $Threads $Parallel