echo TASK_ID=${SGE_TASK_ID}
GEM5_DIR=/u/home/s/seanzw/llvm-trace-cpu
GEM5_BIN=${GEM5_DIR}/build/X86/gem5.opt
GEM5_CONFIG=${GEM5_DIR}/configs/example/llvm_trace_replay.py

TDG=/u/home/s/seanzw/llvm-tdg/benchmark/SPU/scnn/spu.scnn.replay.0.tdg

export LD_LIBRARY_PATH="/u/home/s/seanzw/install/lib:$LD_LIBRARY_PATH"

COMMAND_SH=${SCRATCH}/command.${SGE_TASK_ID}.sh
bash ${COMMAND_SH}

# ${GEM5_BIN} ${GEM5_CONFIG} --llvm-standalone=1 --llvm-issue-width=8 --llvm-store-queue-size=32 --llvm-trace-file=${TDG} --caches --l2cache --cpu-type=DerivO3CPU --num-cpus=1 --l1d_size=32kB --l1i_size=32kB --l2_size=256kB
