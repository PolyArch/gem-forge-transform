import subprocess as sp
import os

offset_bytes = [
    '0B', '64B', '128B', '256B', '512B',
    '1kB', '2kB', '4kB', '8kB', '12kB', '16kB',
    '20kB', '24kB', '28kB', 
    '32kB', '36kB', '40kB', '44kB', '48kB', '52kB', '56kB',
    '60kB', '64kB',
]

for offset in offset_bytes:
    if offset.endswith('kB'):
        offset_value = int(offset[:-2]) * 1024
    else:
        offset_value = int(offset[:-1])

    dir_name = f'omp_dot_prod_avx_offset_{offset}'
    sp.check_call(['mkdir', '-p', dir_name])
    with open(os.path.join(dir_name, f'{dir_name}.c'), 'w') as f:
        f.write(f'#define OFFSET_BYTES {offset_value}\n')
        f.write(f'#include "../omp_dot_prod_avx_offset_base.c"')

    dir_name = f'omp_dot_prod_avx_random{offset}'
    sp.check_call(['mkdir', '-p', dir_name])
    with open(os.path.join(dir_name, f'{dir_name}.c'), 'w') as f:
        f.write(f'#define RANDOMIZE\n')
        f.write(f'#define OFFSET_BYTES {offset_value}\n')
        f.write(f'#include "../omp_dot_prod_avx_offset_base.c"')

    dir_name = f'omp_dot_prod_avx_reverse{offset}'
    sp.check_call(['mkdir', '-p', dir_name])
    with open(os.path.join(dir_name, f'{dir_name}.c'), 'w') as f:
        f.write(f'#define REVERSE\n')
        f.write(f'#define OFFSET_BYTES {offset_value}\n')
        f.write(f'#include "../omp_dot_prod_avx_offset_base.c"')