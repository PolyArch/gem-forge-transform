#!/bin/sh

# rm -f /tmp/job_scheduler.*

run_test () {
    local bench=$1
    local rubyc=$2
    local input=$3
    local threads=$4
    local cpu=$5
    local valid_trans=valid.ex
    local sim_valid_prefix=replay/ruby/single/$cpu.tlb
    local sim_valid=$sim_valid_prefix.${RubyConfig}

    local stream_trans=stream/ex/static/so.store.cmp
    local sim_stream_prefix=stream/ruby/single/$cpu.tlb
    local sim_ssp=$sim_stream_prefix.$rubyc.c

    local trans=$valid_trans,$stream_trans
    local all_sim=$sim_valid,$sim_valid.bingo,$sim_valid.idea,$sim_ssp,$sim_ssp.flts,$sim_ssp-idea
    local parallel=4
    python Driver.py -b $bench \
        -t $trans \
        --sim-configs $all_sim \
        --sim-input $input \
        --input-threads $threads \
        -s -j $parallel 
}

Benchmark=$1
CPU=$2
SimInputName=as-skitter

python Driver.py -b ${Benchmark} --build
python Driver.py -b ${Benchmark} --trace --fake-trace

python Driver.py -b $Benchmark --transform-text -t valid.ex -d

RubyConfig=8x8c
StreamTransform=stream/ex/static/so.store.cmp
python Driver.py -b ${Benchmark} --transform-text -t ${StreamTransform} \
    -d --transform-debug StreamExecutionTransformer,StaticStream

RubyConfig=8x8c
Threads=64
run_test $Benchmark $RubyConfig $SimInputName $Threads $CPU
