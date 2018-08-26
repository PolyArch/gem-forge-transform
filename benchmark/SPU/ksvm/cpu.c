#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define VTYPE float
#define C 0.1
#define tol 0.02
// #define max_passes 100
#define max_passes 1
//#define M 100 // number of instances
//#define N 10 // number of features
#define sigma 0.5
//#define ratio 0.6

#ifndef N
#define N 10
#endif

#ifndef M
#define M 100
#endif

#ifndef ratio
#define ratio 0.2
#endif

inline float min(float a, float b) { return a < b ? a : b; }
inline float max(float a, float b) { return a > b ? a : b; }

// this is like both are sparse- need different definition
// __attribute__((noinline))
double dot_prod_sparse(float *data_val, float *data_ind, int ptr1,
                              int end1, int ptr2, int end2) {
  double accum = 0;
  while (ptr1 <= end1 && ptr2 <= end2) {
    if (data_ind[ptr1] == data_ind[ptr2]) {
      accum += data_val[ptr1] * data_val[ptr2];
      ptr1++;
      ptr2++;
    } else {
      if (data_ind[ptr1] <= data_ind[ptr2])
        ptr1++;
      else
        ptr2++;
    }
  }
  return accum;
}

inline double norm(float *data_val, int start_ptr, int end_ptr) {
  VTYPE output = 0;
  for (int i = start_ptr; i < end_ptr; ++i) {
    output += data_val[i] * data_val[i];
  }
  return output;
}

inline float gauss_kernel(float x) {
  return exp(-(x * x) / (2 * sigma * sigma));
}

void train(float *data_val, float *data_ind, int *row_ptr, double *y) {
  float alpha[M];
  float b1, b2, b = 0; // initial bias?
  for (int i = 0; i < M; ++i) {
    alpha[i] = 0.1;
  }
  double E[M]; // let's see the dim of E

  for (int k = 0; k < M; ++k) {
    E[k] = -y[k];
  }

  float L = 0, H = 0;
  int passes = 0;

  float old_alpha_i = 0, old_alpha_j = 0;
  VTYPE eta = 0;
  float diff = 0;
  int i = 0, j = 0;

  double duality_gap = 0;
  double dual = 0;

  while (passes < max_passes) {
    passes++;

    // cout << "Pass number: " << passes << "\n";
    // Select new i and j such that E[i] is max and E[j] is min
    double max_E = E[i];
    double min_E = E[j];
    for (int k = 1; k < M; ++k) {
      if (E[k] > max_E) {
        i = k;
        max_E = E[k];
      }
      if (E[k] < min_E) {
        j = k;
        min_E = E[k];
      }
    }

    // Step 1:
    old_alpha_i = alpha[i];
    old_alpha_j = alpha[j];
    if (y[i] != y[j]) {
      L = max(0, alpha[j] - alpha[i]);
      H = min(C, C + alpha[j] - alpha[i]);
    } else {
      L = max(0, alpha[i] + alpha[j] - C);
      H = min(C, alpha[i] + alpha[j]);
    }
    // cout << "L=H?\n";
    if (L == H)
      continue;
    VTYPE inter_prod =
        dot_prod_sparse(data_val, data_ind, row_ptr[i], row_ptr[i + 1],
                        row_ptr[j], row_ptr[j + 1]);
    VTYPE intra_prod1 = norm(data_val, row_ptr[i], row_ptr[i + 1]);
    VTYPE intra_prod2 = norm(data_val, row_ptr[j], row_ptr[j + 1]);
    eta = 2 * inter_prod - intra_prod1 - intra_prod2;
    if (eta >= 0)
      eta = 2;

    diff = (y[j] * (E[i] - E[j])) / eta;
    alpha[j] = alpha[j] - diff;

    if (alpha[j] > H) {
      alpha[j] = H;
    } else if (alpha[j] < L) {
      alpha[j] = L;
    }
    // diff = alpha[j]-old_alpha_j;
    /*
    cout << "Diff was less\n";
    if(diff < 1e-5) {
        continue;
    }
    */

    // double diff1 = (y[i]*y[j])*(diff2);
    double diff1 = (y[i] * y[j]) * (diff);
    // alpha[i]=alpha[i]+ (y[i]*y[j])*(old_alpha_j-alpha[j]);
    alpha[i] = alpha[i] + diff1;

    // alpha[i]=alpha[i]+ (y[i]*y[j])*(old_alpha_j-alpha[j]);
    b1 = b - E[i] - y[i] * diff * intra_prod1 - y[j] * diff * inter_prod;
    b2 = b - E[j] - y[i] * diff * inter_prod - y[j] * diff * intra_prod2;
    if (alpha[i] > 0 && alpha[i] < C) {
      b = b1;
    } else if (alpha[j] > 0 && alpha[j] < C) {
      b = b2;
    } else {
      b = (b1 + b2) / 2;
    }

    dual = dual - diff / y[i] * (E[i] - E[j]) +
           eta / 2 * (diff / y[i]) * (diff / y[i]);

    // Step 2: (M is number of instances)
    // calculate all the new parameters
    // cout << "Came to calculate new E[i]\n";

    // kernel error update
    for (int k = 0; k < M; ++k) {
      E[k] = E[k] +
             (diff1)*y[i] * gauss_kernel(dot_prod_sparse(
                                data_val, data_ind, row_ptr[k], row_ptr[k + 1],
                                row_ptr[i], row_ptr[i + 1])) +
             (diff)*y[j] * gauss_kernel(dot_prod_sparse(
                               data_val, data_ind, row_ptr[k], row_ptr[k + 1],
                               row_ptr[j], row_ptr[j + 1]));
      // E[k] = E[k] +
      // (alpha[i]-old_alpha_i)*y[i]*gauss_kernel(dot_prod_sparse(data,
      // row_ptr[k], row_ptr[k+1],row_ptr[i], row_ptr[i+1])) +
      // (alpha[j]-old_alpha_j)*y[j]*gauss_kernel(dot_prod_sparse(data,
      // row_ptr[k], row_ptr[k+1],row_ptr[j], row_ptr[j+1]));
    }

    duality_gap = 0;
    for (int k = 0; k < M; ++k) {
      duality_gap += alpha[k] * y[k] * E[k];
    }
    duality_gap += b;
  }

  // end_roi();
  // cout << "Algorithm converged at number of passes: " << passes << endl;
}

int main() {

  int nnz1 = 0, nrows1 = 0;
  int *row_ptr;
  // std::pair<VTYPE,VTYPE> *matrix1;
  float *data_val;
  float *data_ind;

  double *y;
  y = (double *)malloc(M * sizeof(double));
  data_val = (float *)malloc(M * N * ratio * sizeof(float));
  data_ind = (float *)malloc(M * N * ratio * sizeof(float));
  row_ptr = (int *)malloc((M + 1) * sizeof(int));

  float *temp_val;
  float *temp_ind;
  double *out;
  out = (double *)malloc(1 * sizeof(double));
  temp_val = (float *)malloc(N * ratio * sizeof(float));
  temp_ind = (float *)malloc(N * ratio * sizeof(float));

  int id = 0;

  FILE *m1_file;
  char lineToRead[5000];

  m1_file = fopen("input.data", "r");
  printf("Start reading matrix1\n");

  while (fgets(lineToRead, 500000, m1_file) != NULL) {
    sscanf(lineToRead, "%lf %f:%f %f:%f %f:%f %f:%f %f:%f %f:%f", &out[0],
           &temp_ind[0], &temp_val[0], &temp_ind[1], &temp_val[1], &temp_ind[2],
           &temp_val[2], &temp_ind[3], &temp_val[3], &temp_ind[4], &temp_val[4],
           &temp_ind[5], &temp_val[6]);
    // y[id] = (int32_t)(out[0] * (1<<FxPnt));
    y[id] = out[0];
    for (int j = 0; j < N * ratio; ++j) {
      // data_val[(int)(id*N*ratio + j)] = (int32_t)(temp_val[j] * (1<<FxPnt));
      // data_ind[(int)(id*N*ratio + j)] = (int32_t)(temp_ind[j] * (1<<FxPnt));

      data_val[(int)(id * N * ratio + j)] = temp_val[j];
      data_ind[(int)(id * N * ratio + j)] = temp_ind[j];
    }
    row_ptr[id] = id * N * ratio;
    id++;
  }
  row_ptr[M] = N * M * ratio;

  printf("Finished reading input data\n");

  train(data_val, data_ind, row_ptr, y);
  printf("svm training done\n");

  return 0;
}
