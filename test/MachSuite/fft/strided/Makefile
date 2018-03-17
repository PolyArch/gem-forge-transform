KERN=fft
ALG=strided

# CC=clang

CFLAGS?=-O3 -Wall -Wno-unused-label

SRCS=./$(KERN).c local_support.c ../../common/support.c
FILES=$(SRCS) $(KERN).h ../../common/support.h
INCLUDE_DIR=../../common

#####################################################################################
PASS_SO_DIR=../../../../build/prototype
OPT_ARGS=-load=$(PASS_SO_DIR)/libPrototypePass.so -S -time-passes

ALL_SRCS=$(SRCS) ../../common/harness.c
LLVMS=$(ALL_SRCS:%.c=%.o)
LLVM_TRACES=$(LLVMS:%.ll=%.tll)
LLVM_REPLAYS=$(LLVMS:%.ll=%.rll)

# Compile
%.o: %.c
	clang -flto -I${INCLUDE_DIR} ${CFLAGS} -o $@ -c $^

$(KERN).bc: $(LLVMS)
	clang -flto -Wl,-plugin-opt=emit-llvm $^ -o $@
	# Name everthing
	opt -instnamer $@ -o $@

# Instrument
$(KERN)_trace.ll: $(KERN).bc
	opt $(OPT_ARGS) -debug-only=PrototypePass -prototype -o $@ $^

$(KERN)_replay.ll: $(KERN).bc
	opt $(OPT_ARGS) -debug-only=ReplayPass,DynamicTrace -replay -trace-file=llvm_trace.txt -o $@ $^

# To binary.
$(KERN)_trace: $(KERN)_trace.ll ../../../logger.c
	clang -O0 -o $@ $^

$(KERN)_replay: $(KERN)_replay.ll ../../../replay.c
	clang -O0 -o $@ $^

run_trace: $(KERN)_trace input.data check.data
	./$(KERN)_trace input.data check.data

clean_trace:
	rm -f $(KERN)_trace $(LLVMS) $(LLVM_TRACES) $(KERN)_trace.bc $(KERN)_trace.s

clean_replay:
	rm -f $(KERN)_replay $(LLVMS) $(LLVM_REPLAYS) $(KERN)_replay.bc $(KERN)_replay.s

#####################################################################################

$(KERN): $(FILES) ../../common/harness.c
	$(CC) $(CFLAGS) -I../../common -o $(KERN) $(SRCS) ../../common/harness.c -lm

run: $(KERN) input.data check.data
	./$(KERN) input.data check.data

generate: $(FILES) generate.c
	$(CC) $(CFLAGS) -I../../common -o generate $(SRCS) generate.c -lm
	./generate

hls: $(KERN).c $(KERN).h
	vivado_hls hls.tcl

clean: clean_trace clean_replay
	rm -f $(KERN) generate output.data
