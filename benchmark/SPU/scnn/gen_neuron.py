
import numpy, sys
import random

try:
    Tx, ratio, rle_width, Ni = int(sys.argv[1]), float(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])

except:
    Tx, ratio, rle_width, Ni = int(sys.argv[1]), float(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])

#duplicate for Ni neurons
#n*n=Tx*Ty neuron
def get_neuron():
    nnz = Tx*Tx*ratio
    nz = Tx*Tx*(1-ratio)
    dist_per_elem = (nz/nnz)
    padding = int(nnz%rle_width)
    random.seed(0)
    # print nnz
    # print nz
    # print dist_per_elem
    with open('csr_neuron.data', 'a+') as f_input:
        for j in range(int(nnz)): #last should be padding
            line = []
            line.append('%.2f %d' % (256*random.random(),
                random.randrange(int(j/ratio),int((j+1)/ratio))))
            f_input.write(' '.join(line) + '\n')

        line = []
        if(padding!=0):
            for j in range(padding):
                line.append('%.2f %d' %(256*random.random(), dist_per_elem*4))

            for j in range(4-padding):
                line.append('%.2f %d' %(0, 0))

            f_input.write(' '.join(line) + '\n')


open('csr_neuron.data', 'w').close()
get_neuron()
#generating Tn neurons
# for i in range(Ni):
#     get_neuron()

last_ind = [0] * rle_width
counter=0
data_ind = [0] * rle_width
data_val = [0] * rle_width

# generate RLE encoded neurons: assuming dist would be less than (2^16)
with open('input_neuron.data', 'w') as f_output:
    for i in range(Ni):
        input_file = open('csr_neuron.data','r')
        for line in input_file:
            values = line.split(" ")
            data_val[counter] = float(values[0])
            data_ind[counter] = int(values[1])
            counter = counter+1
            if(counter==rle_width):
                line = []
                for i in range(4):
                    rle_dist = data_ind[i]-last_ind[i]
                    line.append('%.2f %d' %(data_val[i], rle_dist))
                    last_ind[i] = data_ind[i]
                counter=0
                f_output.write(' '.join(line) + '\n')