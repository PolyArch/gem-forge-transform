
ifndef N
N=10
endif

ifndef M
M=100
endif

ifndef ratio
ratio=0.2
endif

TESTS=cpu
TESTC=$(TESTS:=.cpp)

all: $(TESTS)

CPP=riscv64-unknown-elf-g++

MACROS= -DN=$(N) -DM=$(M) -Dratio=$(ratio)

input.data: gen.py
	python $< $(M) $(N) $(ratio)

OPT?=-O3
CFLAGS=$(OPT) -g -ggdb -gdwarf-3 
#--std=c++11 

$(TESTS): % :%.cpp input.data
	 $(CPP) $< $(MACROS) $(LIB) $(CFLAGS) -static -o $@

run:
	/home/vidushi/gem5/build/RISCV/gem5.opt ~/gem5/configs/example/se.py --cpu-type=MinorCPU --l1d_size=64kB --l1i_size=16kB --l2_size=1024kB --caches --cmd=cpu


clean:
	rm -f $(TESTS) *.s *.o

cleandata:
	rm -f *.data

ultraclean: clean cleandata