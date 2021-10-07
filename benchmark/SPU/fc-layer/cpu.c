

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define VTYPE uint16_t

#ifndef N
#define N 18
#endif

#ifndef M
#define M 4096
#endif

#ifndef act_sp
#define act_sp 0.351
#endif

#ifndef syn_sp
#define syn_sp 0.09
#endif

__attribute__((noinline)) void mv_mult(VTYPE *wgt_col_ind, VTYPE *wgt_val,
                                       int *wgt_row_ptr, VTYPE *act_ind,
                                       VTYPE *act_val, VTYPE *out_vec) {

  int ptr1, ptr2, end1, end2;
  ptr1 = 0;
  end1 = 0;
  ptr2 = 0;
  end2 = (int)(M * syn_sp);

  VTYPE accum = 0;

  for (int i = 0; i < N; i++) {

    ptr1 = wgt_row_ptr[i];
    end1 = wgt_row_ptr[i + 1];
    ptr2 = 0;
    accum = 0;

    while (ptr1 <= end1 && ptr2 <= end2) {
      if (wgt_col_ind[ptr1] == act_ind[ptr2]) {
        accum += (VTYPE)(wgt_val[ptr1] * act_val[ptr2]);
        ptr1++;
        ptr2++;
      } else {
        if (wgt_col_ind[ptr1] <= act_ind[ptr2])
          ptr1++;
        else
          ptr2++;
      }
    }
    out_vec[i] = (VTYPE)accum;
  }
}

int main() {

  char lineToRead[5000];

  VTYPE *act_val;
  VTYPE *act_ind;

  int *wgt_row_ptr;
  VTYPE *wgt_col_ind;
  VTYPE *wgt_val;

  // int nnz = (int)(M*act_sp);
  act_val = (VTYPE *)malloc((int)(M * act_sp) * sizeof(VTYPE));
  act_ind = (VTYPE *)malloc((int)(M * act_sp) * sizeof(VTYPE));
  int tind = 0, tval = 0;

  // READING ACTIVATIONS FIRST
  FILE *act_file = fopen("input_activations.data", "r");

  int id = 0;
  printf("Start reading activations file\n");

  while (fscanf(act_file, "%d, %d", &tind, &tval) == 2) {
    act_ind[id] = (VTYPE)tind;
    act_val[id] = (VTYPE)tval;
    id++;
  }

  printf("Done reading activations file\n");
  fclose(act_file);

  wgt_val = (VTYPE *)malloc((int)(N * M * syn_sp) * sizeof(VTYPE));
  wgt_col_ind = (VTYPE *)malloc((int)(N * M * syn_sp) * sizeof(VTYPE));
  wgt_row_ptr = (int *)malloc((N + 1) * sizeof(int));

  int row_id;
  int prev_row_id = -1;

  // READING WEIGHTS NOW
  FILE *weight_file = fopen("input_weights.data", "r");

  id = 0;
  printf("Start reading weights file\n");

  // Empty rows thing won't be a problem here
  while (fscanf(weight_file, "%d %d %d", &row_id, &tind, &tval) == 3) {
    wgt_col_ind[id] = (VTYPE)tind;
    wgt_val[id] = (VTYPE)tval;

    if (row_id != prev_row_id) {
      wgt_row_ptr[row_id] = id;
      prev_row_id = row_id;
    }
    id++;
  }
  // may check it!
  wgt_row_ptr[N] = id;

  printf("Done reading weights file\n");
  fclose(weight_file);

  VTYPE *out_vec;
  out_vec = (VTYPE *)malloc(N * sizeof(VTYPE));

  mv_mult(wgt_col_ind, wgt_val, wgt_row_ptr, act_ind, act_val, out_vec);

  return 0;
}