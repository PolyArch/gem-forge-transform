// Test benchmark from SDVBS.disparity.padarray4.c
typedef struct {
  int height;
  int width;
  int *data;
} I2D;

#define subsref(A, i, j) ((A)->data[(A)->width * (i) + (j)])

__attribute__((noinline)) float foo(I2D *inMat, I2D *borderMat, int dir,
                                    I2D *paddedArray) {
  int rows, cols, bRows, bCols, newRows, newCols;
  int i, j;
  int adir;

  adir = abs(dir);
  rows = inMat->height;
  cols = inMat->width;

  bRows = borderMat->data[0];
  bCols = borderMat->data[1];

  newRows = rows + bRows;
  newCols = cols + bCols;

  if (dir == 1) {
    for (i = 0; i < rows; i++)
      for (j = 0; j < cols; j++)
        subsref(paddedArray, i, j) = subsref(inMat, i, j);
  } else {
    for (i = 0; i < rows - bRows; i++)
      for (j = 0; j < cols - bCols; j++)
        subsref(paddedArray, (bRows + i), (bCols + j)) = subsref(inMat, i, j);
  }
}

int main() {
  const int N = 5;
  I2D inMat, boarderMat, paddedArray;
  inMat.height = N;
  inMat.width = N;
  inMat.data = malloc(sizeof(int) * N * N);
  boarderMat.data = malloc(sizeof(int) * 2);
  boarderMat.data[0] = boarderMat.data[1] = N;
  paddedArray.width = N + N;
  paddedArray.height = N + N;
  paddedArray.data = malloc(sizeof(int) * N * N * 4);
  foo(&inMat, &boarderMat, 1, &paddedArray);
  free(inMat.data);
  free(boarderMat.data);
  return 0;
}
