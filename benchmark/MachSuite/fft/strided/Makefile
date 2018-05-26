KERN=fft
ALG=strided

# CC=clang

override CFLAGS:=$(CFLAGS) -Wall -Wno-unused-label

SRCS=./$(KERN).c local_support.c ../../common/support.c
FILES=$(SRCS) $(KERN).h ../../common/support.h
INCLUDE_DIR=../../common

#####################################################################################
ALL_SRCS=$(SRCS) ../../common/harness.c
ROOT_DIR=../../../..
LINK_FLAGS=-lm

include ../../Makefile.TDG.mk

#####################################################################################

# $(KERN): $(FILES) ../../common/harness.c
# 	$(CC) $(CFLAGS) -I../../common -o $(KERN) $(ALL_SRCS) $(LINK_FLAGS)

run: $(KERN) input.data check.data
	./$(KERN) input.data check.data

generate: $(FILES) generate.c
	$(CC) $(CFLAGS) -I../../common -o generate $(SRCS) generate.c -lm
	./generate

hls: $(KERN).c $(KERN).h
	vivado_hls hls.tcl

clean:
	rm -f $(KERN) generate output.data
