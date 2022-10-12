
import subprocess

# BY can only be four as we need 64 threads to process 256 rows.

tile_sizes = [
	(4, 8),
	(4, 16),
	(4, 32),
	(4, 64),
	(4, 128),
	(4, 256),
]

workload_names = list()

for by, bx in tile_sizes:

    template = f"""
#define LOOP_ORDER LOOP_ORDER_BY_BX_O_Y_X_I
#define BY {by}
#define BX {bx}
#include "../../omp_conv3d_zxy_oyxi_outer_tile_avx/omp_conv3d_zxy_oyxi_outer_tile_avx.c"
    """

    folder_name = f'omp_conv3d_zxy_byboxyxi_outer_{by}x{bx}_avx'
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
