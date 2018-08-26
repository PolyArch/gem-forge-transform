ifndef N
N=18
endif

ifndef M
M=4096
endif


ifndef act_sp
act_sp=0.351
endif

ifndef syn_sp
syn_sp=0.09
endif

TESTS=cpu
TESTC=$(TESTS:=.cpp)

all: $(TESTS)

CPP=riscv64-unknown-elf-g++
MACROS= -DN=$(N) -DM=$(M) -Dact_sp=$(act_sp) -Dsyn_sp=$(syn_sp) 
GCC=g++

OPT?=-O3
CFLAGS=$(OPT) -g -ggdb -gdwarf-3 
#--std=c++11 
LFLAGS = -lm

input_activations.data: gen_activations.py 
	python $< $(M) $(act_sp)

input_weights.data: gen_weights.py 
	python $< $(N) $(M) $(syn_sp)

input.data: input_weights.data input_activations.data
	echo "input.data"

$(TESTS): % :%.cpp input_activations.data input_weights.data
	 $(CPP) $< $(LIB) $(CFLAGS) $(MACROS) -static -o $@ $(LFLAGS)

run:
	/home/vidushi/gem5/build/RISCV/gem5.opt ~/gem5/configs/example/se.py --cpu-type=MinorCPU --l1d_size=64kB --l1i_size=16kB --l2_size=1024kB --caches --cmd=cpu

cleandata:
	rm -f *.data

clean:
	rm -f $(TESTS) *.s *.o

ultraclean: cleandata cleancpu clean
	rm -f *.log