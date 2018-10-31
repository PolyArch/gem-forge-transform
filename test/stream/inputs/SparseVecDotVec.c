#define N 9

int a_idx[N];
int b_idx[N];
float a_val[N];
float b_val[N];

float foo(int *a_idx, int *b_idx, float *a_val, float *b_val) {
  int i = 0, j = 0;
  float sum = 0.0f;
  while (i < N && j < N) {
    int ai = a_idx[i];
    int bj = b_idx[j];
    if (ai == bj) {
      sum += a_val[i] * b_val[i];
      i++;
      j++;
    } else if (ai < bj) {
      i++;
    } else {
      j++;
    }
  }
  return sum;
}

int main() {
  foo(a_idx, b_idx, a_val, b_val);
  return 0;
}