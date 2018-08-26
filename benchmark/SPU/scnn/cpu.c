
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Define the parameters if not defined externally
#ifndef Sy
#define Sy 1
#define Sx 1
#endif

#ifndef Tx
#define Tx 32
#endif

#ifndef Kx
#define Kx 3
#endif

#ifndef Ni
#define Ni 64
#endif

#ifndef core_per_spad
#define core_per_spad 4
#endif

#ifndef Tn
#define Tn 1
#endif

#ifndef neuron_sp
#define neuron_sp 0.25
#endif

#ifndef synapse_sp
#define synapse_sp 0.45
#endif

#ifndef rle_width
#define rle_width 4
#endif

#define VTYPE uint16_t

#define Kx 3
#define Ky 3
#define Nx 224
#define Ny 224
// #define Nn 64
#define Nn 64

#define Kxsim                                                                  \
  (Kx | Kx << 16 | (Kx & 0xFFFFFFFFFFFFFFFF) << 32 |                           \
   (Kx & 0xFFFFFFFFFFFFFFFF) << 48)
#define Nxsim                                                                  \
  (Nx | Nx << 16 | (Nx & 0xFFFFFFFFFFFFFFFF) << 32 |                           \
   (Nx & 0xFFFFFFFFFFFFFFFF) << 48)

// #define Ni 64
// #define tile_factor 8
// #define Ni 64
#define tile_factor 1
// #define tile_factor 1
// #define Tn 1 // for now

// #define NYPAD (Ny+Ky)
// #define NXPAD (Nx+Kx)

#define NYPAD (Ny)
#define NXPAD (Nx)
#define Ti (Ni / core_per_spad)
// #define Ti (Ni)

#define NYSCL (Ny / Sy)
#define NXSCL (Nx / Sx)

#define SYNAPSE_SIZE (1L * Ky * Kx * Ni)

// #define nnz_syn (synapse_sp * Kx * Ky * Tn)
// #define nnz1 (synapse_sp * Kx * Ky * Tn)
// #define nnz2 NYPAD*NXPAD*Ni*sparsity_n
// #define nnz_ne (neuron_sp * Tx * Tx)
// #define nnz2 (25076 * Ni)

const int nnz_syn = synapse_sp * Kx * Ky * Tn;
const int nnz1 = synapse_sp * Kx * Ky * Tn;
const int nnz_ne = neuron_sp * Tx * Tx;
const int nnz2 = 25076 * Ni;

void fill_convolution_data(VTYPE synapse_val[Ni][nnz1],
                           VTYPE synapse_ind[Ni][nnz1],
                           VTYPE neuron_i_val[nnz2], VTYPE neuron_i_ind[nnz2],
                           VTYPE neuron_ptr[Ni * tile_factor + 1]) {

  char lineToRead[1000];
  int id = 0;
  // FILE* neuron_file = fopen("dataset_rle.txt","r");
  FILE *neuron_file = fopen("input_neuron.data", "r");

  while (fgets(lineToRead, 5000, neuron_file) != NULL) {
    // this is the number of values in the rle_width
    char ignore;

    for (int i = 0; i < rle_width; ++i) {
      float a;
      int b;
      sscanf(lineToRead, "%f %d", &a, &b);
      // std::cout << a << " " << b << "\n";
      // neuron_i_ind[id] = (int16_t)(b * (1 << 8));
      neuron_i_ind[id] = b;
      neuron_i_val[id] = (uint16_t)(a * (1 << 8));
      // iss >> neuron_i_val[id] >> neuron_i_ind[id];

      // std::cout << neuron_i_val[id] << " " << neuron_i_ind[id] << "\n";
      id++;
    }
  }
  fclose(neuron_file);
  // for neuron: id is nnz2 now
  for (int i = 0; i < tile_factor * Ni; ++i) {
    neuron_ptr[i] = (id * i) / (tile_factor * Ni);
  }
  neuron_ptr[Ni * tile_factor] = id;

  // nnz1 = 4*Nn*Ni;
  // srand(1);
  // synapse_val[Ni][Kx*Ky*Tn];
  id = 0;
  FILE *synapse_file = fopen("input_synapse.data", "r");
  while (fgets(lineToRead, 5000, synapse_file) != NULL) {

    // std::string raw(lineToRead);
    // std::istringstream iss(raw.c_str());
    // float a,b;
    // iss >> a >> b;

    // synapse_ind[0][id] = (uint16_t)(b);
    // synapse_val[0][id] = (uint16_t)(a * (1 << 8));

    sscanf(lineToRead, "%hu %hu", &synapse_val[0][id], &synapse_ind[0][id]);
    id++;
  }
  fclose(synapse_file);
  /*
  while(fgets(lineToRead, 5000, synapse_file) != NULL) {
    sscanf(lineToRead, "%hu %hu", &synapse_val[0][id], &synapse_ind[0][id]);
    id++;
  }
  */
  // duplicate for Ni feature maps
  for (int j = 1; j < Ni; ++j) {
    for (int i = 0; i < id; ++i) {
      synapse_val[j][i] = synapse_val[0][i];
      synapse_ind[j][i] = synapse_ind[0][i];
    }
  }
}

// std::pair<int,int> convolution_layer_blocked((VTYPE
// (&synapse_val)[nnz1][Nn],VTYPE (&synapse_ind)[nnz1][Nn],
__attribute__((noinline)) void
convolution_layer_blocked(VTYPE synapse_val[Ni][nnz1],
                          VTYPE synapse_ind[Ni][nnz1], VTYPE neuron_i_val[nnz2],
                          VTYPE neuron_i_ind[nnz2],
                          VTYPE neuron_ptr[Ni * tile_factor + 1],
                          VTYPE neuron_n[NYSCL * NXSCL * Nn]) {

  // int c1=0,c2=0;
  // VTYPE sum[Nn]={0};
  int size_neuron_tile = 0;
  int size_synapse = 0;
  int num_comp_inst = 0;
  VTYPE sy_ind, sy_val;
  VTYPE ny_ind, ny_val;
  VTYPE out_prod, out_ind, out_ind_prev = Nx;

  for (int tile_no = 0; tile_no < tile_factor; ++tile_no) {
    for (int feature_map_id = 0; feature_map_id < Ti; ++feature_map_id) {

      size_synapse = nnz_syn;
      size_neuron_tile = nnz_ne; // size of 1 tile
      int nval_st = neuron_ptr[(feature_map_id * tile_factor + tile_no)];

      for (int nn = 0; nn < Tn; nn++) {

        size_synapse = (size_synapse / 4) * 4;
        size_neuron_tile = (size_neuron_tile / 4) * 4;
        num_comp_inst = (size_synapse * size_neuron_tile) / 4;

        for (int weight = 0; weight < size_synapse / Tn; ++weight) {
          sy_ind = synapse_ind[feature_map_id][nn + weight];
          sy_val = synapse_val[feature_map_id][nn + weight];
          out_ind_prev = Nx - (sy_ind / Kx + Nx * (sy_ind % Kx));
          for (int is = 0; is < size_neuron_tile; ++is) {
            ny_ind = neuron_i_ind[nval_st + is];
            ny_val = neuron_i_val[nval_st + is];

            out_prod = sy_val * ny_val;
            out_ind_prev += ny_ind;

            neuron_n[out_ind_prev] += out_prod;
          }
        }
      }
    }
  }
}

int main() {

  VTYPE(*synapse_val)[Ni][nnz1];
  VTYPE(*synapse_ind)[Ni][nnz1];
  VTYPE(*neuron_i_val)[nnz2];
  VTYPE(*neuron_i_ind)[nnz2];
  VTYPE(*neuron_ptr)[Ni * tile_factor + 1];
  VTYPE(*neuron_n)[NYSCL * NXSCL * Nn];

  printf("allocating memory\n");

  synapse_val = (VTYPE(*)[Ni][nnz1])malloc(nnz1 * Ni * sizeof(VTYPE));
  synapse_ind = (VTYPE(*)[Ni][nnz1])malloc(nnz1 * Ni * sizeof(VTYPE));
  neuron_i_val = (VTYPE(*)[nnz2])malloc(nnz2 * sizeof(VTYPE));
  neuron_i_ind = (VTYPE(*)[nnz2])malloc(nnz2 * sizeof(VTYPE));
  neuron_ptr = (VTYPE(*)[Ni * tile_factor + 1])
      malloc((Ni + 1) * tile_factor * sizeof(VTYPE));
  neuron_n = (VTYPE(*)[(NYSCL * NXSCL * Nn)])
      malloc(NYSCL * NXSCL * Nn * sizeof(VTYPE));

  printf("initializing arrays\n");

  fill_convolution_data(*synapse_val, *synapse_ind, *neuron_i_val,
                        *neuron_i_ind, *neuron_ptr);

  printf("starting computation\n");

  //   convolution_layer_blocked(*synapse_val, *synapse_ind, *neuron_i_val,
  //                             *neuron_i_ind, *neuron_ptr, *neuron_n);
  // Blocked Version
  convolution_layer_blocked(*synapse_val, *synapse_ind, *neuron_i_val,
                            *neuron_i_ind, *neuron_ptr, *neuron_n);

  printf("blocked computation complete!\n");

  // compare((VTYPE*)*neuron_n,(VTYPE*)*neuron_n2,NYSCL*NXSCL*Nn);

  printf("done\n");
  return 0;
}