import sys
import random

try:
    m, ratio = int(sys.argv[1]), float(sys.argv[2])
except:
    m, ratio = int(sys.argv[1]), float(sys.argv[2])

with open('input_activations.data', 'w') as f_input:
    nnz = int(m*ratio)
    line = []
    prev_ind = 0
    for i in range(nnz):
        #generate ith non-zero value
        upper = int(i/ratio)
        if(prev_ind!=upper):
            ind = random.randint(prev_ind+1, int(i/ratio))
        else:
            ind=prev_ind
        line.append('%d %d\n' % ( ind , random.randint(1,500)))
        prev_ind = ind
    f_input.write(''.join(line))