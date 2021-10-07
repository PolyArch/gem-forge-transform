import sys
import random

def main():
    data_type = sys.argv[1]
    if data_type == 'int64_t' or data_type == 'uint64_t':
        data_size = 8
    elif data_type == 'int32_t' or data_type == 'uint32_t':
        data_size = 4
    else:
        print(f'Unknown data type {data_type}')
    size = int(eval(sys.argv[2]))
    N = size // data_size
    if size > 1024 * 1024:
        size_string = f'{size//1024//1024}MB'
    elif size > 1024:
        size_string = f'{size//1024}kB'
    else:
        size_string = f'{size}B'

    print(f'Generate {data_type} {size_string} = {data_size} * {N}')
    data = list(range(N))
    prefix = 'linear'
    if len(sys.argv) > 3:
        prefix = 'linear_shuffled'
        random.shuffle(data)

    with open(f'{prefix}.{data_type}.{size_string}.c', 'w') as f:
        f.write(f'static {data_type} {prefix}_{size_string}[{N}] = {{\n')
        f.write(', '.join([str(v) for v in data]))
        f.write('\n};')

if __name__ == '__main__':
    main()