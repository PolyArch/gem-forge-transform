KERN=viterbi
ALG=viterbi

override CFLAGS:=$(CFLAGS) -Wall -Wno-unused-label -fno-inline-functions

SRCS=$(KERN).c local_support.c ../../common/support.c
FILES=$(SRCS) $(KERN).h ../../common/support.h
INCLUDE_DIR=../../common

#####################################################################################
ROOT_DIR=../../../..
ALL_SRCS=$(SRCS) ../../common/harness.c

include ../../Makefile.TDG.mk

#####################################################################################


# $(KERN): $(FILES) ../../common/harness.c
# 	$(CC) $(CFLAGS) -I../../common -o $(KERN) $(SRCS) ../../common/harness.c

run: $(KERN) input.data check.data
	./$(KERN) input.data check.data

generate: $(FILES) generate.c
	$(CC) $(CFLAGS) -I../../common -o generate $(SRCS) generate.c -lm
	./generate

hls: $(KERN).c $(KERN).h
	vivado_hls hls.tcl

clean:
	rm -f $(KERN) generate output.data
