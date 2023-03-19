#ifndef AFFINITY_ALLOC_HH
#define AFFINITY_ALLOC_HH

#include "stdlib.h"

extern "C" {

/**
 * @brief Alloc the block closer to a list of addresses.
 *
 * @param size Bytes to allocate.
 * @param ... A list of affinity addresses.
 * @return void*
 */
void *malloc_aff(size_t size, int n, const void **affinity_addrs);

/**
 * @brief Free the affinity allocated data. Currently do nothing.
 *
 * @param ptr
 */
void free_aff(void *ptr);
}

#endif