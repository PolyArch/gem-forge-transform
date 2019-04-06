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

__attribute__((noinline)) F2D *foo(F2D *in, size_type startRow,
                                   size_type numberRows,
                                   size_type startCol,
                                   size_type numberCols) {
  size_type i, j, k, rows, cols;
  F2D *out;

  rows = numberRows + startRow;
  cols = numberCols + startCol;
  out = fMalloc(numberCols, numberRows);

  k = 0;
  for (i = startRow; i < rows; i++)
    for (j = startCol; j < cols; j++)
      asubsref(out, k++) = subsref(in, i, j);

  return out;
}

int main() {
  F2D *in = fMalloc(5, 5);
  F2D *out = foo(in, 0, 5, 0, 5);
  fFree(out);
  fFree(in);
}