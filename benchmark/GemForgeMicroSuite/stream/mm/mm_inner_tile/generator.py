import subprocess

tile_sizes = [
	(2, 2, 1024),
	(4, 4, 512),
	(8, 8, 256),
	(16, 16, 128),
	(32, 32, 64),
]

workload_names = list()

for tile_l, tile_n, tile_m in tile_sizes:

    template = f"""
#define TILE_L {tile_l}
#define TILE_M {tile_m}
#define TILE_N {tile_n}
#include "../../omp_mm_inner_avx/omp_mm_inner_avx.c"
    """

    folder_name = f'omp_mm_inner_tile{tile_l}x{tile_n}x{tile_m}_avx'
    subprocess.check_call([
        'mkdir',
        '-p',
        folder_name
    ])
    source_name = f'{folder_name}/{folder_name}.c'
    with open(source_name, 'w') as f:
        f.write(template)

    workload_name = f'gfm.{folder_name}'
    workload_names.append(workload_name)

for wn in workload_names:
    print(wn)
