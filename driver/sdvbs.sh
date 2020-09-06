#!/bin/sh

# rm -f /tmp/job_scheduler.*

Benchmark=sdvbs.disparity
# Benchmark=sdvbs.localization
# Benchmark=sdvbs.mser
# Benchmark=sdvbs.multi_ncut
# Benchmark=sdvbs.sift
# Benchmark=sdvbs.stitch
# Benchmark=sdvbs.svm
# Benchmark=sdvbs.texture_synthesis
# Benchmark=sdvbs.tracking
# Benchmark=sdvbs.disparity,sdvbs.localization,sdvbs.mser,sdvbs.multi_ncut,sdvbs.sift,sdvbs.stitch,sdvbs.svm,sdvbs.texture_synthesis,sdvbs.tracking
SimInput=fullhd

# python Driver.py -b $Benchmark --build
# python Driver.py -b $Benchmark --profile
python Driver.py -b $Benchmark --simpoint --simpoint-mode=region
# python Driver.py -b $Benchmark --trace --fake-trace

# python Driver.py -b $Benchmark -t valid.ex -d
RubyConfig=8x8c
sim_replay_prefix=replay/ruby/single
i4=$sim_replay_prefix/i4.tlb.${RubyConfig}
o4=$sim_replay_prefix/o4.tlb.${RubyConfig}
o8=$sim_replay_prefix/o8.tlb.${RubyConfig}
sim_replay=$o8.bingo-l2pf
# python Driver.py -b $Benchmark -t valid.ex --sim-input-size $SimInput \
#     --sim-configs $sim_replay -s &
    # --gem5-debug SyscallBase

StreamTransform=stream/ex/static/so.store
# python Driver.py -b $Benchmark -t $StreamTransform -d 

run_ssp () {
    local bench=$1
    local trans=$2
    local rubyc=$3
    local input=$4
    local threads=$5
    local parallel=$6
    local i4=stream/ruby/single/i4.tlb.$rubyc.c
    local o4=stream/ruby/single/o4.tlb.$rubyc.c-gb-fifo
    local o8=stream/ruby/single/o8.tlb.$rubyc.c-gb-fifo
    local o8_link=stream/ruby/single/o8.tlb.$rubyc-link.c
    # local all_sim=${o8_link}-gb-fifo.flts-mc
    local all_sim=$o8
    python Driver.py -b $bench -t $trans \
        --sim-configs $all_sim \
        --sim-input $input \
        --input-threads $threads \
        -s -j $parallel &
        # --gem5-debug O3CPUDelegator,StreamEngine,StreamBase --gem5-debug-start 232900000000 --gem5-max-ticks 233000000000 | tee tracking.log
        # --gem5-debug RubyStreamLife | tee bfs.log &
}
RubyConfig=8x8c
Threads=1
Parallel=1
# run_ssp $Benchmark $StreamTransform $RubyConfig $SimInput $Threads $Parallel