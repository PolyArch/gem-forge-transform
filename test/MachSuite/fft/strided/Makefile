KERN=fft
ALG=strided

CFLAGS?=-O3 -Wall -Wno-unused-label

SRCS=./$(KERN).c local_support.c ../../common/support.c
FILES=$(SRCS) $(KERN).h ../../common/support.h
INCLUDE_DIR=../../common

#####################################################################################
PASS_SO_DIR=../../../../build/prototype
OPT_DEBUG=PrototypePass
OPT_ARGS=-load=$(PASS_SO_DIR)/libPrototypePass.so -S -time-passes -debug-only=${OPT_DEBUG} 

ALL_SRCS=$(SRCS) ../../common/harness.c
LLVMS=$(ALL_SRCS:%.c=%.ll)
LLVM_TRACES=$(LLVMS:%.ll=%.tll)
LLVM_REPLAYS=$(LLVMS:%.ll=%.rll)

# Compile
%.ll: %.c
	clang -emit-llvm -S -I$(INCLUDE_DIR) $(CFLAGS) -o $@ -c $^

# Instrument
%.tll: %.ll
	opt $(OPT_ARGS) -prototype -o $@ $^

%.rll: %.ll
	opt $(OPT_ARGS) -replay -o $@ $^

# Link together.
$(KERN)_trace.bc: $(LLVM_TRACES) ../../../logger.ll
	llvm-link -o $@ $^

$(KERN)_replay.bc: $(LLVM_REPLAYS) ../../../replay.ll
	llvm-link -o $@ $^

# To asm.
$(KERN)_trace.s: $(KERN)_trace.bc
	llc -filetype=asm -o $@ $^

$(KERN)_replay.s: $(KERN)_replay.bc
	llc -filetype=asm -o $@ $^

# To binary.
$(KERN)_trace: $(KERN)_trace.s
	gcc -fno-inline -o $@ $^

$(KERN)_replay: $(KERN)_replay.s
	gcc -fno-inline -o $@ $^

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
