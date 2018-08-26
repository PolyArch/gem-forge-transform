
#import numpy, 
import sys
import random

try:
    n, m, ratio = int(sys.argv[1]), int(sys.argv[2]), float(sys.argv[3])
except:
    n, m, ratio = int(sys.argv[1]), int(sys.argv[2]), float(sys.argv[3])

with open('input.data', 'w') as f_input:
    for i in range(n):
        random.seed(i)
        line = []
        line.append('%d ' % (random.choice([-1, 1])))
        for j in range(int(m*ratio)):
            line.append('%d:%.2f ' % ( random.randint(int((j)/ratio),
                        int((j+1)/ratio-1)), random.random()))
        f_input.write(' '.join(line) + '\n')