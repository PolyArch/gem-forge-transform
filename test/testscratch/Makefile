KERN=scratch

CFLAGS?=-O3 -Wall -Wno-unused-label -fno-inline

SRCS=./main.c
FILES=$(SRCS)
INCLUDE_DIR=.
TEST_ROOT_DIR=..


#####################################################################################
PASS_SO_DIR=$(TEST_ROOT_DIR)/../build/prototype
LOGGER_DIR=$(TEST_ROOT_DIR)/../prototype
OPT_ARGS=-load=$(PASS_SO_DIR)/libPrototypePass.so -S -time-passes

ALL_SRCS=$(SRCS)
LLVMS=$(ALL_SRCS:%.c=%.o)

# Compile
%.o: %.c
	clang -flto -I${INCLUDE_DIR} ${CFLAGS} -o $@ -c $^

# Link time optimization
$(KERN).bc: $(LLVMS)
	clang -flto -Wl,-plugin-opt=emit-llvm $^ -o $@
	# Name everthing
	opt -instnamer $@ -o $@

# Instrument
$(KERN)_trace.ll: $(KERN).bc
	opt $(OPT_ARGS) -debug-only=PrototypePass -prototype -o $@ $^

$(KERN)_replay.ll: $(KERN).bc
	opt $(OPT_ARGS) -debug-only=ReplayPass,DynamicTrace -stream-replay -trace-file=llvm_trace.gz -o $@ $^

# To binary.
$(KERN): $(KERN).bc
	clang -O0 -o $@ $^

$(KERN)_trace: $(KERN)_trace.ll $(LOGGER_DIR)/logger.c
	clang -O0 $^ -lz -o $@

$(KERN)_replay: $(KERN)_replay.ll $(TEST_ROOT_DIR)/replay.c
	clang -O0 -o $@ $^

run_trace: $(KERN)_trace
	./$(KERN)_trace

clean_trace:
	rm -f $(LLVMS) $(KERN)_trace.ll $(KERN).bc $(KERN)_trace

clean_replay:
	rm -f $(LLVMS) $(KERN)_replay.ll $(KERN).bc $(KERN)_replay

#####################################################################################
clean: clean_trace clean_replay
	rm -f $(KERN)
