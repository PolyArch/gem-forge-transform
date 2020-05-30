#ifndef GEM_FORGE_MICRO_SUITE_UTILS_H
#define GEM_FORGE_MICRO_SUITE_UTILS_H

// Used to readin an array and warm up the cache.
#define CACHE_LINE_SIZE 64
#define WARM_UP_ARRAY(a, n)                                                    \
  for (int i = 0; i < sizeof(*(a)) * (n); i += CACHE_LINE_SIZE) {              \
    volatile char x = ((char *)(a))[i];                                        \
  }

#endif