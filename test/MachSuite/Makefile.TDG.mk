# Includer should define
# $ROOT_DIR
# $ALL_SRCS
# $INCLUDE_DIR
# $CFLAGS
# $KERN
# $LINK_FLAG

CXX=clang++
override CFLAGS:=${CFLAGS} -O3
CCA_MAX_FUS?=3

BUILD_DIR=$(ROOT_DIR)/build/prototype
GEM5_DIR=/home/sean/Public/gem5
OPT_ARGS=-load=$(BUILD_DIR)/libLLVMTDGPass.so -S

LLVMS=$(ALL_SRCS:%.c=%.o)

REPLAY_INCLUDE_DIR=$(INCLUDE_DIR) -I$(GEM5_DIR)/include

TRACER_FILE_NAME=llvm_trace

# Link the gzip tracer.
# TRACE_LINK_FLAGS=$(LINK_FLAG) -lz
# TRACER_LIB=$(BUILD_DIR)/libTracerGZip.a
# TRACE_FILE_FORMAT=gzip
# Link the protobuf tracer.
TRACE_LINK_FLAGS=$(LINK_FLAG) -lprotobuf
TRACER_LIB=$(BUILD_DIR)/libTracerProtobuf.a
TRACE_FILE_FORMAT=protobuf


# Compile
%.o: %.c
	clang -flto -I${INCLUDE_DIR} ${CFLAGS} -o $@ -c $^

$(KERN).bc: $(LLVMS)
	clang -flto -Wl,-plugin-opt=emit-llvm $^ -o $@
	# Name everthing
	opt -instnamer $@ -o $@

$(KERN): $(KERN).bc
	clang -O0 $^ -o $@ $(LINK_FLAG)

# Instrument
$(KERN)_trace.ll: $(KERN).bc
	opt $(OPT_ARGS) -debug-only=TracePass -trace-pass -trace-function=$(KERN) -o $@ $^

$(KERN)_replay.ll: $(KERN).bc
	opt $(OPT_ARGS) -debug-only=ReplayPass,DynamicTrace -replay -trace-file=$(TRACER_FILE_NAME) -trace-format=$(TRACE_FILE_FORMAT) $^ -o $@ 

$(KERN)_cca.ll: $(KERN).bc
	opt $(OPT_ARGS) -debug-only=ReplayPass,DynamicTrace -replay-cca -trace-file=$(TRACER_FILE_NAME) -cca-max-fus=$(CCA_MAX_FUS) $^ -o $@ 

$(KERN)_cca_analysis.ll : $(KERN).bc
	opt $(OPT_ARGS) -debug-only=ReplayPass,DynamicTrace -stream-analysis-trace -stream-trace-file=$(TRACER_FILE_NAME) -analyze $^

# To binary.
# Trace is linked with CXX as the trace library is in cxx.
$(KERN)_trace: $(KERN)_trace.ll $(TRACER_LIB)
	$(CXX) -O0 $^ -o $@ $(TRACE_LINK_FLAGS)

$(KERN)_replay: $(KERN)_replay.ll ${ROOT_DIR}/test/replay.c
	clang -c -O0 $(KERN)_replay.ll -o ${KERN}_replay_before_gcc.o
	# Must use gcc to link gem5 pseudo instruction asm.
	gcc -O0 -I${GEM5_DIR}/include ${ROOT_DIR}/test/replay.c ${GEM5_DIR}/util/m5/m5op_x86.S $(KERN)_replay_before_gcc.o -o $@ ${LINK_FLAG}

$(KERN)_cca: $(KERN)_cca.ll ${ROOT_DIR}/test/replay.c
	clang -c -O0 $(KERN)_cca.ll -o ${KERN}_replay_before_gcc.o
	# Must use gcc to link gem5 pseudo instruction asm.
	gcc -O0 -I${GEM5_DIR}/include ${ROOT_DIR}/test/replay.c ${GEM5_DIR}/util/m5/m5op_x86.S $(KERN)_replay_before_gcc.o -o $@ ${LINK_FLAG}

run_trace: $(KERN)_trace input.data check.data
	./$(KERN)_trace input.data check.data

clean_trace:
	rm -f $(KERN)_trace $(LLVMS) ${KERN}_trace.ll ${KERN}.bc

clean_replay:
	rm -f $(KERN)_replay $(KERN)_cca $(LLVMS) $(KERN)_replay.ll $(KERN)_cca.ll ${KERN}.bc ${KERN}_replay_before_gcc.S
