import sys
import random
import argparse
import array

parser = argparse.ArgumentParser(description='Generate Kmeans Input.')
parser.add_argument('--points', type=int,
                    help='number of points')
parser.add_argument('--dims', type=int,
                    help='number of dims')
parser.add_argument('--data-type', default='float',
                    help='data type')

args = parser.parse_args()

def main():
    data_type = args.data_type
    if data_type == 'int64_t' or data_type == 'uint64_t':
        data_size = 8
    elif data_type in ['int32_t', 'uint32_t', 'float']:
        data_size = 4
    else:
        print(f'Unknown data type {data_type}')
    points = args.points
    dims = args.dims
    size = points * dims * data_size
    N = points * dims
    if size > 1024 * 1024:
        size_string = f'{size//1024//1024}MB'
    elif size > 1024:
        size_string = f'{size//1024}kB'
    else:
        size_string = f'{size}B'

    print(f'Generate {data_type} {size_string} = {data_size} * {points}x{dims}')
    data = list(range(N))
    if data_type == 'float':
        for i in range(N):
            data[i] = random.random()
        data_array = array.array('f', data)
    else:
        for i in range(N):
            data[i] = random.randint(0, 255)
        assert(False)

    prefix = 'features'
    with open(f'{prefix}.{data_type}.{points}.{dims}.data', 'wb') as f:
        data_array.tofile(f)

if __name__ == '__main__':
    main()