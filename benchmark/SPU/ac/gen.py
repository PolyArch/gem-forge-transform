# import numpy
import sys
import random

try:
    d1, d2, ratio = int(sys.argv[1]), int(sys.argv[2]), float(sys.argv[3])
except:
    d1, d2, ratio = int(sys.argv[1]), int(sys.argv[2]), float(sys.argv[3])

with open('input.data', 'w') as f_input:
    random.seed(0)
    d = d2 - d1 + 1
    ind_list = [0] * d
    # can parameterize this
    # ind_list[0] = pow(2,d1)*0.5
    # for i in range(5):
    #     ind_list[i] = pow(2,i)

    mul_turn = True
    # root node
    ind_list[0] = 1
    for l in range(5):
        line = []
        line.append('level%d' % (l))
        line.append('%d' % (ind_list[l]))
        if(mul_turn):
            line.append('*')
            mul_turn = False
        else:
            line.append('+')
            mul_turn = True
        print ind_list[l]
        for j in range(ind_list[l]):
            # child1
            line.append('%d:%d' % (l+1, ind_list[l+1]))
            ind_list[l+1] = ind_list[l+1] + 1
            # child2
            line.append('%d:%d' % (l+1, ind_list[l+1]))
            ind_list[l+1] = ind_list[l+1] + 1
        f_input.write(' '.join(line) + '\n')

    # ind_list[0] = pow(2,d1)*ratio
    # should map only till before last level
    for l in range(5, d-1):
        line = []
        line.append('level%d' % (l))
        line.append('%d' % (ind_list[l]))
        if(mul_turn):
            line.append('*')
            mul_turn = False
        else:
            line.append('+')
            mul_turn = True
        print ind_list[l]
        for j in range(int(ind_list[l]*ratio)):
            for k in range(int(1/ratio)-1):
                # line.append('level: %d index: %d' % (l, ind_list[l+1]))
                # child 1
                line.append('%d:%d' % (l+1, ind_list[l+1]))
                ind_list[l+1] = ind_list[l+1] + 1
                # child 2
                line.append('%d:%d' % (l+1, ind_list[l+1]))
                ind_list[l+1] = ind_list[l+1] + 1

            if(l+2 > d-1):
                rand_level = l+1
            else:
                # rand_level = random.randint(l+2, d-1)
                rand_level = l+2
            # line.append('level: %d index: %d' % (rand_level, random.randint(0,
            # child 1
            line.append('%d:%d' % (l, ind_list[l+1]))
            ind_list[l+1] = ind_list[l+1] + 1
            # child 2 (this would always be 0? no, i need to update it later
            # on...)
            # line.append('%d:%d' % (rand_level, random.randint(0, ind_list[rand_level])))
            # line.append('%d:%d' % (rand_level, j))
            # assuming number of nodes at each level is at least not going to
            # decrease
            line.append('%d:%d' % (rand_level, random.randint(0, ind_list[l])))
            # line.append('%d:%d' % (rand_level, random.randint(0,
            #    pow(2,rand_level+d1)*ratio*ratio)))
        f_input.write(' '.join(line) + '\n')
