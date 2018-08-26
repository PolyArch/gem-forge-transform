ifndef d1
d1=1
endif

ifndef d2
d2=8
endif

ifndef ratio
ratio=0.5
endif

ifndef B
B=1
endif

ifndef cores_per_spad
cores_per_spad=4
endif

ifndef num_pe
num_pe=64
endif

TESTS=cpu
TESTC=$(TESTS:=.cpp)

all: $(TESTS)

MACROS= -Dd1=$(d1) -Dd2=$(d2) -Dratio=$(ratio) -DB=$(B) -Dcores_per_spad=$(cores_per_spad) -Dnum_pe=$(num_pe)

input.data: gen.py
	python $< $(d1) $(d2) $(ratio) 

OPT?=-O3
CFLAGS=$(OPT) -g -ggdb -gdwarf-3 
#--std=c++11 

$(TESTS): % :%.cpp input.data
	 $(CPP) $< $(MACROS) $(LIB) $(CFLAGS) -static -o $@

cleandata:
	rm -f *.data

clean:
	rm -f $(TESTS) *.s *.o

ultraclean: clean cleandata