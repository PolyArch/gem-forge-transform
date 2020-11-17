void stream_memset(char *dest, char val, long long len) {
#pragma clang loop vectorize_width(64) unroll_count(8)
  for (long long i = 0; i < len; ++i) {
    dest[i] = val;
  }
}

void stream_memcpy(char *dest, const char *src, long long len) {
#pragma clang loop vectorize_width(64) unroll_count(8)
  for (long long i = 0; i < len; ++i) {
    char val = src[i];
    dest[i] = val;
  }
}

void stream_memmove(char *dest, const char *src, long long len) {
  if (dest < src || dest >= src + len) {
#pragma clang loop vectorize_width(64) unroll_count(8)
    for (long long i = 0; i < len; ++i) {
      char val = src[i];
      dest[i] = val;
    }
  } else if (dest != src) {
#pragma clang loop vectorize_width(64) unroll_count(8)
    for (long long i = len - 1; i >= 0; --i) {
      char val = src[i];
      dest[i] = val;
    }
  }
}