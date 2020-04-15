/**
 * Simple array sum.
 */
#ifdef GEM_FORGE
#include "gem5/m5ops.h"
#endif

#include <malloc.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "immintrin.h"

typedef float Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

#define Nx 256
#define Ny 256
#define Ni 16
#define Nn 64
#define Kx 3
#define Ky 3
#define Px (Kx - 1)
#define Py (Ky - 1)
#define NxPad (Nx + Px)
#define NyPad (Ny + Py)
#define Bx 32
#define By 32
#define BxPad (Bx + Px)
#define ByPad (By + Py)

float hsum_ps_sse1(__m128 v) { // v = [ D C | B A ]
  __m128 shuf = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1)); // [ C D | A B ]
  __m128 sums = _mm_add_ps(v, shuf); // sums = [ D+C C+D | B+A A+B ]
  shuf = _mm_movehl_ps(shuf, sums);  //  [   C   D | D+C C+D ]  // let the
                                     //  compiler avoid a mov by reusing shuf
  sums = _mm_add_ss(sums, shuf);
  return _mm_cvtss_f32(sums);
}

__attribute__((noinline)) Value foo(Value *I, Value *K, Value *O) {

// #pragma clang loop vectorize(disable)
#pragma omp parallel for schedule(static)
  for (uint64_t n = 0; n < Nn; ++n) {
    for (uint64_t yy = 0; yy < Ny; yy += By) {
      for (uint64_t xx = 0; xx < Nx; xx += Bx) {
        Value localSum[ByPad][BxPad] = {0};
        for (uint64_t y = 0; y < By; ++y) {
          for (uint64_t x = 0; x < Bx; ++x) {

            const uint64_t idxI = (yy + y) * Nx * Ni + (xx + x) * Ni;
#if Ni == 4
            __m128 valI = _mm_load_ps(I + idxI);
#elif Ni == 16
            __m512 valI = _mm512_load_ps(I + idxI);
#else
#error "Illegal Ni"
#endif
            for (uint64_t ky = 0; ky < Ky; ++ky) {
              for (uint64_t kx = 0; kx < Kx; ++kx) {

                const uint64_t idxK = n * Kx * Ky * Ni + ky * Kx * Ni + kx * Ni;
#if Ni == 4
                __m128 valK = _mm_load_ps(K + idxK);
                __m128 valM = _mm_mul_ps(valK, valI);
                // Reduction over the partial sum.
                Value sum = hsum_ps_sse1(valM);
#elif Ni == 16
                __m512 valK = _mm512_load_ps(K + idxK);
                __m512 valM = _mm512_mul_ps(valK, valI);
                Value sum = _mm512_reduce_add_ps(valM);
#endif

                // Wierd padding.
                Value valS = localSum[y + Py - ky][x + Px - kx];
                localSum[y + Py - ky][x + Px - kx] = valS + sum;
              }
            }
          }
        }

        // Write back (no activation so far as we are faking with integer
        // type).
        for (uint64_t y = 0; y < ByPad; ++y) {
          for (uint64_t x = 0; x < BxPad; ++x) {
            const uint64_t idxO =
                n * NyPad * NxPad + (yy + y) * NxPad + (xx + x);
            O[idxO] = localSum[y][x];
          }
        }
      }
    }
  }
  return 0.0f;
}

#define CACHE_BLOCK_SIZE 64

int main(int argc, char *argv[]) {

  int numThreads = 1;
  if (argc == 2) {
    numThreads = atoi(argv[1]);
  }
  printf("Number of Threads: %d.\n", numThreads);

  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);

  size_t sizeI = Nx * Ny * Ni * sizeof(Value);
  size_t sizeK = Nn * Kx * Ky * Ni * sizeof(Value);
  size_t sizeO = Nn * NxPad * NyPad * sizeof(Value);

  Value *I = (Value *)aligned_alloc(CACHE_BLOCK_SIZE, sizeI);
  Value *K = (Value *)aligned_alloc(CACHE_BLOCK_SIZE, sizeK);
  Value *O = (Value *)aligned_alloc(CACHE_BLOCK_SIZE, sizeO);

  printf("Input %lukB, Weight %lukB, Output %lukB.\n", sizeI / 1024,
         sizeK / 1024, sizeO / 1024);

  if (!I || !K || !O) {
    printf("Failed to allocate I %p K %p O %p.\n", I, K, O);
    exit(1);
  }

#ifdef GEM_FORGE
  m5_detail_sim_start();
#endif
#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < sizeI; i += CACHE_BLOCK_SIZE / sizeof(Value)) {
    volatile Value x = ((uint8_t *)I)[i];
  }
  for (long long i = 0; i < sizeK; i += CACHE_BLOCK_SIZE / sizeof(Value)) {
    volatile Value x = ((uint8_t *)K)[i];
  }
  for (long long i = 0; i < sizeO; i += CACHE_BLOCK_SIZE / sizeof(Value)) {
    volatile Value y = ((uint8_t *)O)[i];
  }
  // Start the threads.
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    printf("Start thread %d.\n", tid);
  }
#endif

#ifdef GEM_FORGE
  m5_reset_stats(0, 0);
#endif

  volatile Value c = foo(I, K, O);
#ifdef GEM_FORGE
  m5_detail_sim_end();
  exit(0);
#endif

#ifdef CHECK
  // Value expected = 0;
  // for (int i = 0; i < N; i += STRIDE) {
  //   expected += a[i];
  // }
  // expected *= NUM_THREADS;
  // printf("Ret = %d, Expected = %d.\n", c, expected);
#endif

  return 0;
}
