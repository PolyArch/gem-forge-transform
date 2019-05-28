#include <stdlib.h>

typedef int size_type;

typedef struct {
  size_type height;
  size_type width;
  float *data;
} F2D;

#define subsref(A, i, j) ((A)->data[(A)->width * (i) + (j)])
#define asubsref(A, i) ((A)->data[(i)])

void fFree(F2D *in) {
  free(in->data);
  free(in);
}

F2D *fMalloc(int width, int height) {
  F2D *ret = malloc(sizeof(F2D));
  ret->width = width;
  ret->height = height;
  ret->data = malloc(ret->width * ret->height * sizeof(float));
  return ret;
}

__attribute__((noinline)) F2D *foo(F2D *in) {
  size_type width = in->width;
  size_type height = in->height;
  F2D *symmat = fMalloc(in->width, in->width);
  float *data = symmat->data;
  float *in_data = in->data;
  for (size_type i = 0; i < width; ++i) {
    for (size_type j = i; j < width; ++j) {
      const size_type out_idx = i * width + j;
      data[out_idx] = 0.0;
      for (size_type k = 0; k < height; ++k) {
        const size_type in_idx1 = k * width + i;
        const size_type in_idx2 = k * width + j;
        data[out_idx] += in_data[in_idx1] * in_data[in_idx2];
      }
    }
  }
  return symmat;
}

#define WIDTH 16
#define HEIGHT 1024

int main() {
  F2D *in = fMalloc(WIDTH, HEIGHT);
  F2D *out = foo(in);
  fFree(out);
  fFree(in);
}