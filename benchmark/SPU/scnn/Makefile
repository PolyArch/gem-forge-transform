ifndef Tx
Tx=32
#Tx=14
endif

ifndef Kx
Kx=3
endif

ifndef Ni
Ni=64
endif

ifndef core_per_spad
core_per_spad=4
endif

ifndef Tn
Tn=1
endif

ifndef neuron_sp
neuron_sp=0.25
endif

ifndef synapse_sp
synapse_sp=0.45
endif

ifndef rle_width
rle_width=4
endif

TESTS=cpu
TESTC=$(TESTS:=.cpp)

all: $(TESTS)

CPP=riscv64-unknown-elf-g++
MACROS= -DTx=$(Tx) -DNi=$(Ni) -DTn=$(Tn) -Dneuron_sp=$(neuron_sp) -Dsynapse_sp=$(synapse_sp) -Drle_width=$(rle_width) -Dcore_per_spad=$(core_per_spad)

OPT?=-O3
CFLAGS=$(OPT) -g -ggdb -gdwarf-3 
#--std=c++11 
LFLAGS = -lm

input_neuron.data: gen_neuron.py 
	python $< $(Tx) $(neuron_sp) $(rle_width) $(Ni)

input_synapse.data: gen_synapse.py 
	python $< $(Kx) $(synapse_sp) $(Ni) $(Tn)

input.data: input_neuron.data input_synapse.data
	echo "input.data"

$(TESTS): % :%.cpp  input_neuron.data input_synapse.data
	 $(CPP) $< $(LIB) $(CFLAGS) $(MACROS) -static -o $@ $(LFLAGS)

run:
	/home/vidushi/gem5/build/RISCV/gem5.opt ~/gem5/configs/example/se.py --cpu-type=MinorCPU --l1d_size=64kB --l1i_size=16kB --l2_size=1024kB --caches --cmd=cpu


cleandata:
	rm -f input_neuron.data input_synapse.data

clean:
	rm -f $(TESTS)  *.s *.o

ultraclean: cleandata cleancpu clean
	rm -f *.log