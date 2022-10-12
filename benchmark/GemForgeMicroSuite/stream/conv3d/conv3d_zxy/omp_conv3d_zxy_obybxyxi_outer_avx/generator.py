
import subprocess

tile_sizes = [
	(8, 8),
	(8, 16),
	(16, 16),
	(16, 32),
	(32, 32),
	(32, 64),
	(64, 64),
	(64, 128),
	(128, 128),
	(128, 256),
	(256, 256),
]

workload_names = list()

for by, bx in tile_sizes:

    template = f"""
#define LOOP_ORDER LOOP_ORDER_O_BY_BX_Y_X_I
#define BY {by}
#define BX {bx}
#include "../../omp_conv3d_zxy_oyxi_outer_tile_avx/omp_conv3d_zxy_oyxi_outer_tile_avx.c"
    """

    folder_name = f'omp_conv3d_zxy_obybxyxi_outer_{by}x{bx}_avx'
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
