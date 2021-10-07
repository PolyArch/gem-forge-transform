ifndef N
N=1000
endif

ifndef Mt
Mt=4
endif

ifndef k
k=64
endif

ifndef ratio
ratio=0.25
endif

ifndef M
M=4
endif

TESTS=cpu
TESTC=$(TESTS:=.cpp)

all: $(TESTS)

CPP=riscv64-unknown-elf-g++
MACROS = -DN=$(N) -DMt=$(Mt) -Dk=$(k) -Dratio=$(ratio) -DM=$(M)


OPT?=-O3
CFLAGS=$(OPT) -g -ggdb -gdwarf-3 
#--std=c++11 
LFLAGS = -lm

input.data: gen.py
	python $< $(N) $(Mt) $(k) $(ratio)

$(TESTS): % :%.cpp input.data
	 $(CPP) $< $(LIB) $(CFLAGS) $(MACROS) -static -o $@ $(LFLAGS)

run:
	/home/vidushi/gem5/build/RISCV/gem5.opt ~/gem5/configs/example/se.py --cpu-type=MinorCPU --l1d_size=64kB --l1i_size=16kB --l2_size=1024kB --caches --cmd=cpu


cleandata:
	rm -f input.data inst_id.data

clean:
	rm -f $(TESTS) *.s *.o

ultraclean:
	rm -f $(TESTS) *.s *.o *.data