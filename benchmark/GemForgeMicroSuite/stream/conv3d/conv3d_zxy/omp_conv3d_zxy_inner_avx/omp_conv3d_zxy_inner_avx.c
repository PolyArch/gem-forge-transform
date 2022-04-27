/**
 * ZXY means the data layout --
 *  Z (channel) is the inner-most dimension.
 *  Y is the inner-most dimension,
 *
 * This version is optimized for PUM, in which we:
 * 1. Unroll Kx, Ky.
 */
#include "gfm_utils.h"

#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "immintrin.h"

typedef float Value;

__attribute__((noinline)) Value foo(Value *I, Value *K, Value *O, int64_t Nx,
                                    int64_t Ny, int64_t Ni, int64_t Nn,
                                    int64_t Kx, int64_t Ky) {

  __builtin_assume(Nn > 0);

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)                                      \
    firstprivate(I, K, O, Nx, Ny, Ni, Nn, Kx, Ky)
#else
#pragma clang loop unroll(disable) interleave(disable)
#endif
  for (int64_t o = 0; o < Nn; ++o) {

    // Move these assumption inside the loop so that OpenMP can capture it.
    __builtin_assume(Ny > 3);
    __builtin_assume(Nx > 3);
    __builtin_assume(Ni >= 16);
    __builtin_assume(Ni % 16 == 0);

#pragma clang loop unroll(disable) interleave(disable)
    for (int64_t y = 0; y < Ny - 2; ++y) {

#pragma clang loop unroll(disable) interleave(disable)
      for (int64_t x = 0; x < Nx - 2; ++x) {

        Value s = 0;

#ifndef NO_AVX
#pragma clang loop unroll(disable) interleave(disable) vectorize_width(16)
#else
#pragma clang loop unroll(disable) interleave(disable) vectorize(disable)
#endif
        for (int64_t i = 0; i < Ni; ++i) {

          int64_t idxI = y * Nx * Ni + x * Ni + i;

          Value valI00 = I[idxI + 0 * Ni];
          Value valI01 = I[idxI + 1 * Ni];
          Value valI02 = I[idxI + 2 * Ni];

          Value valI10 = I[idxI + Nx * Ni + 0 * Ni];
          Value valI11 = I[idxI + Nx * Ni + 1 * Ni];
          Value valI12 = I[idxI + Nx * Ni + 2 * Ni];

          Value valI20 = I[idxI + 2 * Nx * Ni + 0 * Ni];
          Value valI21 = I[idxI + 2 * Nx * Ni + 1 * Ni];
          Value valI22 = I[idxI + 2 * Nx * Ni + 2 * Ni];

          int64_t idxK = o * Kx * Ky * Ni + i;

          Value valK00 = K[idxK + 0 * Ni];
          Value valK01 = K[idxK + 1 * Ni];
          Value valK02 = K[idxK + 2 * Ni];

          Value valK10 = K[idxK + Kx * Ni + 0 * Ni];
          Value valK11 = K[idxK + Kx * Ni + 1 * Ni];
          Value valK12 = K[idxK + Kx * Ni + 2 * Ni];

          Value valK20 = K[idxK + 2 * Kx * Ni + 0 * Ni];
          Value valK21 = K[idxK + 2 * Kx * Ni + 1 * Ni];
          Value valK22 = K[idxK + 2 * Kx * Ni + 2 * Ni];

          Value valI0 = valI00 * valK00 + valI01 * valK01 + valI02 * valK02;
          Value valI1 = valI10 * valK10 + valI11 * valK11 + valI12 * valK12;
          Value valI2 = valI20 * valK20 + valI21 * valK21 + valI22 * valK22;

          Value valI = valI0 + valI1 + valI2;
          s += valI;
        }

        // We write to (x, y, o)
        int64_t idxO = y * Nx * Nn + x * Nn + o;
        O[idxO] = s;
      }
    }
  }

  return 0;
}

__attribute__((noinline)) Value driver(Value *I, Value *K, Value *O, int64_t Nx,
                                       int64_t Ny, int64_t Ni, int64_t Nn,
                                       int64_t Kx, int64_t Ky) {
  assert(Kx == 3);
  assert(Ky == 3);
  assert(Ni % 16 == 0);
  return foo(I, K, O, Nx, Ny, Ni, Nn, Kx, Ky);
}

#include "../conv3d_zxy_main.c"