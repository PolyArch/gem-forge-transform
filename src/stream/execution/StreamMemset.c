void stream_memset(char *dest, char val, long long len) {
#pragma clang loop vectorize_width(64) unroll_count(8)
  for (long long i = 0; i < len; ++i) {
    dest[i] = val;
  }
}