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

#define Bx 32
#define By 32
#define Px 2
#define Py 2
#define BxPad (Bx + Px)
#define ByPad (By + Py)

__attribute__((noinline)) Value foo(Value *I, Value *K, Value *O, int64_t Nx,
                                    int64_t Ny, int64_t Ni, int64_t Nn,
                                    int64_t Kx, int64_t Ky) {

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)                                      \
    firstprivate(I, K, O, Nx, Ny, Ni, Nn, Kx, Ky)
#else
#pragma clang loop unroll(disable) interleave(disable)
#endif
  for (int64_t yy = 0; yy < Ny - By + 1; yy += By) {

    // Move these assumption inside the loop so that OpenMP can capture it.
    __builtin_assume(Nn > 0);
    __builtin_assume(Ny > 3);
    __builtin_assume(Nx > 3);
    __builtin_assume(Ni >= 16);
    __builtin_assume(Ni % 16 == 0);

#pragma clang loop unroll(disable) interleave(disable)
    for (int64_t xx = 0; xx < Nx - Bx + 1; xx += Bx) {

#pragma clang loop unroll(disable) interleave(disable)
      for (int64_t o = 0; o < Nn; ++o) {

#ifdef NO_AVX
#error "Tiling requires AVX-512 for vectorization."
#endif
        const int64_t vElem = 16;
        Value localSum[ByPad][BxPad][vElem];
        for (int64_t y = 0; y < ByPad; ++y) {
          for (int64_t x = 0; x < BxPad; ++x) {
            _mm512_store_ps(localSum[y][x], _mm512_set1_ps(0.0f));
          }
        }

#pragma clang loop unroll(disable) interleave(disable)
        for (int64_t y = 0; y < By; ++y) {
#pragma clang loop unroll(disable) interleave(disable)
          for (int64_t x = 0; x < Bx; ++x) {

#define localSumPtr(kx, ky) localSum[y + Py - ky][x + Px - kx]
            __m512 valS00 = _mm512_load_ps(localSumPtr(0, 0));
            __m512 valS01 = _mm512_load_ps(localSumPtr(0, 1));
            __m512 valS02 = _mm512_load_ps(localSumPtr(0, 2));
            __m512 valS10 = _mm512_load_ps(localSumPtr(1, 0));
            __m512 valS11 = _mm512_load_ps(localSumPtr(1, 1));
            __m512 valS12 = _mm512_load_ps(localSumPtr(1, 2));
            __m512 valS20 = _mm512_load_ps(localSumPtr(2, 0));
            __m512 valS21 = _mm512_load_ps(localSumPtr(2, 1));
            __m512 valS22 = _mm512_load_ps(localSumPtr(2, 2));

            for (int64_t i = 0; i < Ni; i += vElem) {

              int64_t idxI = (yy + y) * Nx * Ni + (xx + x) * Ni + i;

              __m512 valI = _mm512_load_ps(I + idxI);

              int64_t idxK = o * Kx * Ky * Ni + i;

              __m512 valK00 = _mm512_load_ps(K + idxK + 0 * Ni);
              __m512 valK01 = _mm512_load_ps(K + idxK + 1 * Ni);
              __m512 valK02 = _mm512_load_ps(K + idxK + 2 * Ni);
              __m512 valK10 = _mm512_load_ps(K + idxK + Kx * Ni + 0 * Ni);
              __m512 valK11 = _mm512_load_ps(K + idxK + Kx * Ni + 1 * Ni);
              __m512 valK12 = _mm512_load_ps(K + idxK + Kx * Ni + 2 * Ni);
              __m512 valK20 = _mm512_load_ps(K + idxK + 2 * Kx * Ni + 0 * Ni);
              __m512 valK21 = _mm512_load_ps(K + idxK + 2 * Kx * Ni + 1 * Ni);
              __m512 valK22 = _mm512_load_ps(K + idxK + 2 * Kx * Ni + 2 * Ni);

              __m512 valM00 = _mm512_mul_ps(valK00, valI);
              __m512 valM01 = _mm512_mul_ps(valK01, valI);
              __m512 valM02 = _mm512_mul_ps(valK02, valI);
              __m512 valM10 = _mm512_mul_ps(valK10, valI);
              __m512 valM11 = _mm512_mul_ps(valK11, valI);
              __m512 valM12 = _mm512_mul_ps(valK12, valI);
              __m512 valM20 = _mm512_mul_ps(valK20, valI);
              __m512 valM21 = _mm512_mul_ps(valK21, valI);
              __m512 valM22 = _mm512_mul_ps(valK22, valI);

              valS00 = _mm512_add_ps(valM00, valS00);
              valS01 = _mm512_add_ps(valM01, valS01);
              valS02 = _mm512_add_ps(valM02, valS02);
              valS10 = _mm512_add_ps(valM10, valS10);
              valS11 = _mm512_add_ps(valM11, valS11);
              valS12 = _mm512_add_ps(valM12, valS12);
              valS20 = _mm512_add_ps(valM20, valS20);
              valS21 = _mm512_add_ps(valM21, valS21);
              valS22 = _mm512_add_ps(valM22, valS22);
            }
            _mm512_store_ps(localSumPtr(0, 0), valS00);
            _mm512_store_ps(localSumPtr(0, 1), valS01);
            _mm512_store_ps(localSumPtr(0, 2), valS02);
            _mm512_store_ps(localSumPtr(1, 0), valS10);
            _mm512_store_ps(localSumPtr(1, 1), valS11);
            _mm512_store_ps(localSumPtr(1, 2), valS12);
            _mm512_store_ps(localSumPtr(2, 0), valS20);
            _mm512_store_ps(localSumPtr(2, 1), valS21);
            _mm512_store_ps(localSumPtr(2, 2), valS22);
          }
        }

        // Write back (no activation so far).
        // Be careful with the boundary case.
        int64_t xStart = xx == 0 ? 1 : 0;
        int64_t xEnd = (xx + Bx == Nx) ? BxPad - 1 : BxPad;
        int64_t yStart = yy == 0 ? 1 : 0;
        int64_t yEnd = (yy + By == Ny) ? ByPad - 1 : ByPad;
        for (int64_t y = yStart; y < yEnd; ++y) {
          for (int64_t x = xStart; x < xEnd; ++x) {
#pragma ss stream_name "gfm.conv3d_zxy_oyxi_outer_tile.buf.ld/nest=2"
            __m512 valS = _mm512_load_ps(localSum[y][x]);
            int64_t idxO = (yy + y - 1) * Nx * Nn + (xx + x - 1) * Nn + o;
            Value sum = _mm512_reduce_add_ps(valS);
#pragma ss stream_name "gfm.conv3d_zxy_oyxi_outer_tile.o.st/nest=2"
            O[idxO] = sum;
          }
        }
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