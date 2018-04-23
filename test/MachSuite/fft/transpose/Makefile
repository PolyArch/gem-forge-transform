KERN=fft
ALG=transpose

override CFLAGS:=$(CFLAGS) -O3 -Wall -Wno-unused-label

SRCS=$(KERN).c local_support.c ../../common/support.c
FILES=$(SRCS) $(KERN).h ../../common/support.h
INCLUDE_DIR=../../common
LINK_FLAG=-lm

#####################################################################################
ROOT_DIR=../../../..
ALL_SRCS=$(SRCS) ../../common/harness.c

include ../../Makefile.TDG.mk

#####################################################################################

# $(KERN): $(FILES) ../../common/harness.c
# 	$(CC) $(CFLAGS) -I../../common -o $(KERN) $(SRCS) ../../common/harness.c $(LINK_FLAG)

run: $(KERN) input.data check.data
	./$(KERN) input.data check.data

generate: $(FILES) generate.c
	$(CC) $(CFLAGS) -I../../common -o generate $(SRCS) generate.c $(LINK_FLAG)

hls: $(KERN).c $(KERN).h
	vivado_hls hls.tcl

clean: clean_replay clean_trace
	rm -f $(KERN) generate output.data
