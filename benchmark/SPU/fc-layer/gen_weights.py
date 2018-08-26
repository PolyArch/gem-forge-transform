import sys
import random

try:
    n, m, ratio = int(sys.argv[1]), int(sys.argv[2]), float(sys.argv[3])
except:                                              
    n, m, ratio = int(sys.argv[1]), int(sys.argv[2]), float(sys.argv[3])

with open('input_weights.data', 'w') as f_input:
    col_nnz = int(m*ratio)
    for j in range(n):
        line = []
        prev_ind = 0
        for i in range(col_nnz):
            #generate ith non-zero value
            random.seed(i)
            static_sparse_id = int(i/ratio)
            if(prev_ind != static_sparse_id):
                ind = random.randint(prev_ind+1, static_sparse_id)
            else:
                ind = prev_ind
            line.append('%d %d %d\n' % (j, ind , random.randint(1,500)))
            prev_ind = ind
        f_input.write(''.join(line))