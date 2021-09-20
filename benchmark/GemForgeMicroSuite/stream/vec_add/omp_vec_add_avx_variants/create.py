import subprocess as sp
import os

offset_bytes = [
    '128kB', '256kB', '512kB', '1MB', '2MB', '4MB', '8MB'
]

static_schedule_chunk_sizes = [
    '0B', '4kB', '8kB', '16kB'
]

CACHE_LINE_SIZE = 64

base_name = 'omp_vec_add_avx'

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