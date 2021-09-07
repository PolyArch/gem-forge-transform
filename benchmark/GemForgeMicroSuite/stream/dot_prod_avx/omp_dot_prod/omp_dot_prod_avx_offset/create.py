import subprocess as sp
import os

offset_bytes = [
    '0B', '64B', '128B', '256B', '512B',
    '1kB', '2kB', '4kB', '8kB', '12kB', '16kB',
    '20kB', '24kB', '28kB', 
    '32kB', '36kB', '40kB', '44kB', '48kB', '52kB', '56kB',
    '60kB', '64kB',
    '128kB', '256kB', '512kB', '1MB', '2MB', '4MB', '8MB',
]

static_schedule_chunk_sizes = [
    '0B', '4kB', '8kB', '16kB'
]

CACHE_LINE_SIZE = 64

base_name = 'omp_dot_prod_avx'

def toBytes(s):
    if s.endswith('MB'):
        return int(s[:-2]) * 1024 * 1024
    elif s.endswith('kB'):
        return int(s[:-2]) * 1024
    else:
        return int(s[:-1])

for static_sched in static_schedule_chunk_sizes:
    static_sched_bytes = toBytes(static_sched)
    static_sched_iters = static_sched_bytes // CACHE_LINE_SIZE
    
    for offset in offset_bytes:
        offset_byte = toBytes(offset)

        dir_name = f'{base_name}_sch_s{static_sched}_offset_{offset}'
        sp.check_call(['mkdir', '-p', dir_name])
        with open(os.path.join(dir_name, f'{dir_name}.c'), 'w') as f:
            f.write(f'#define STATIC_CHUNK_SIZE {static_sched_iters}\n')
            f.write(f'#define OFFSET_BYTES {offset_byte}\n')
            f.write(f'#include "../../{base_name}/{base_name}.c"')

    # dir_name = f'omp_dot_prod_avx_random{offset}'
    # sp.check_call(['mkdir', '-p', dir_name])
    # with open(os.path.join(dir_name, f'{dir_name}.c'), 'w') as f:
    #     f.write(f'#define RANDOMIZE\n')
    #     f.write(f'#define OFFSET_BYTES {offset_value}\n')
    #     f.write(f'#include "../omp_dot_prod_avx_offset_base.c"')

    # dir_name = f'omp_dot_prod_avx_reverse{offset}'
    # sp.check_call(['mkdir', '-p', dir_name])
    # with open(os.path.join(dir_name, f'{dir_name}.c'), 'w') as f:
    #     f.write(f'#define REVERSE\n')
    #     f.write(f'#define OFFSET_BYTES {offset_value}\n')
    #     f.write(f'#include "../omp_dot_prod_avx_offset_base.c"')