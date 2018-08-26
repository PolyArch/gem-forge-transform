import numpy, sys
import random

try:
    Kx, ratio, Ni, Tn = int(sys.argv[1]), float(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])

except:
    Kx, ratio, Ni, Tn = int(sys.argv[1]), float(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])

#duplicate for Ni synapses
#Kx*Ky*Tn weights and duplicate for Ni
def get_synapse():
    nnz = Kx*Kx*ratio
    # nz = Kx*Kx*Tn*(1-ratio)
    # dist_per_elem = int(nz/nnz)
    random.seed(0)
    with open('csr_synapse.data', 'a+') as f_input:
        for j in range(int(nnz)):
            line = []
            line.append('%.2f %d' % (256*random.random(),
                random.randrange(int(j/ratio),int((j+1)/ratio))))
            f_input.write(' '.join(line) + '\n')

        
open('csr_synapse.data', 'w').close()
get_synapse()
#generating Tn neurons
# for i in range(Ni):
#     get_synapse()

with open('input_synapse.data', 'w') as f_output:
    for i in range(Tn):
        last_ind = 0
        input_file = open('csr_synapse.data','r')
        for line in input_file:
            values = line.split(" ")
            data_val = float(values[0])
            data_ind = int(values[1])
            rle_dist = data_ind-last_ind
            last_ind = data_ind
            wr_line = []
            wr_line.append('%.2f %d' % (data_val, rle_dist))
            f_output.write(' '.join(wr_line) + '\n')

