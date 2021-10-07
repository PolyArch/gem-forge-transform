import subprocess as sp
import os

offset_bytes = [
    '4kB', '8kB', '12kB', '16kB',
    '32kB', '60kB', '64kB', '128kB', '256kB',
]

for offset in offset_bytes:
    if offset.endswith('kB'):
        offset_value = int(offset[:-2]) * 1024
    else:
        offset_value = int(offset[:-1])

    dir_name = f'omp_dot_prod_avx_interleave_{offset}'
    sp.check_call(['mkdir', '-p', dir_name])
    with open(os.path.join(dir_name, f'{dir_name}.c'), 'w') as f:
        f.write(f'#define INTERLEAVE_BYTES {offset_value}\n')
        f.write(f'#include "../omp_dot_prod_avx_interleave_base.c"')
