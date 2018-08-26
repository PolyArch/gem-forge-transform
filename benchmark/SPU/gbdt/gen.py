import numpy, sys
import random

try:
    n, m, k, ratio = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]), float(sys.argv[4])

except:
    n, m, k, ratio = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]), float(sys.argv[4])

def get_instance():
    line = []
    for j in range(m):
        line.append('%d' % (random.randint(1,k)))
    line.append('%.2f %.2f' % (random.random(), random.random()))
    return line


with open('input.data', 'w') as f_input:
    for i in range(n):
        random.seed(i)
        f_input.write(' '.join(get_instance()) + '\n')

with open('inst_id.data', 'w') as f_input:
    for i in range(int(n*ratio)):
        f_input.write(str(random.randint(int(i/ratio), int((i+1)/ratio))) + '\n')