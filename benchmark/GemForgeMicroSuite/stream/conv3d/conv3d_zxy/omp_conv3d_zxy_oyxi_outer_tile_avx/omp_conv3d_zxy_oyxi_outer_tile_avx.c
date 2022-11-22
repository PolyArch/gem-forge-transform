/**
 * ZXY means the data layout --
 *  Z (channel) is the inner-most dimension.
 *  Y is the inner-most dimension,
 *
 */
#include "gfm_utils.h"

#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "immintrin.h"

typedef ValueT Value;

#ifndef Bx
#define Bx 32
#define By 32
#endif

#define Px 2
#define Py 2
#define BxPad (Bx + Px)
#define ByPad (By + Py)

#define LOOP_ORDER_BY_BX_O_Y_X_I 0
#define LOOP_ORDER_BYBX_O_Y_X_I 1
#define LOOP_ORDER_O_BY_BX_Y_X_I 1

#ifndef LOOP_ORDER
#define LOOP_ORDER LOOP_ORDER_BY_BX_O_Y_X_I
#endif

__attribute__((noinline)) Value foo(Value *I, Value *K, Value *O, int64_t Nx,
                                    int64_t Ny, int64_t Ni, int64_t Nn,
                                    int64_t Kx, int64_t Ky) {

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)                                      \
    firstprivate(I, K, O, Nx, Ny, Ni, Nn, Kx, Ky)
#else
#pragma clang loop unroll(disable) interleave(disable)
#endif

#if LOOP_ORDER == LOOP_ORDER_BY_BX_O_Y_X_I

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

#elif LOOP_ORDER == LOOP_ORDER_BYBX_O_Y_X_I

  for (int64_t yyxx = 0; yyxx < (Ny / By) * (Nx / Bx); yyxx++) {

    int64_t yy = (yyxx / (Nx / Bx)) * By;
    int64_t xx = (yyxx % (Nx / Bx)) * Bx;

    // Move these assumption inside the loop so that OpenMP can capture it.
    __builtin_assume(Nn > 0);
    __builtin_assume(Ny > 3);
    __builtin_assume(Nx > 3);
    __builtin_assume(Ni >= 16);
    __builtin_assume(Ni % 16 == 0);

#pragma clang loop unroll(disable) interleave(disable)
    for (int64_t o = 0; o < Nn; ++o) {

#elif LOOP_ORDER == LOOP_ORDER_O_BY_BX_Y_X_I
  for (int64_t o = 0; o < Nn; ++o) {

    // Move these assumption inside the loop so that OpenMP can capture it.
    __builtin_assume(Nn > 0);
    __builtin_assume(Ny > 3);
    __builtin_assume(Nx > 3);
    __builtin_assume(Ni >= 16);
    __builtin_assume(Ni % 16 == 0);

#pragma clang loop unroll(disable) interleave(disable)
    for (int64_t yy = 0; yy < Ny - By + 1; yy += By) {

#pragma clang loop unroll(disable) interleave(disable)
      for (int64_t xx = 0; xx < Nx - Bx + 1; xx += Bx) {

#else
#error "Unknown LOOP_ORDER"
#endif

#ifdef NO_AVX
#error "Tiling requires AVX-512 for vectorization."
#endif
        const int64_t vElem = 64 / sizeof(Value);
        Value localSum[ByPad][BxPad][vElem];
        for (int64_t y = 0; y < ByPad; ++y) {
          for (int64_t x = 0; x < BxPad; ++x) {
            ValueAVXStore(localSum[y][x], ValueAVXSet1(0.0f));
          }
        }

#pragma clang loop unroll(disable) interleave(disable)
        for (int64_t y = 0; y < By; ++y) {
#pragma clang loop unroll(disable) interleave(disable)
          for (int64_t x = 0; x < Bx; ++x) {

#define localSumPtr(kx, ky) localSum[y + Py - ky][x + Px - kx]

#pragma ss stream_name "gfm.conv3d_zxy_outer_tile.local.ld00/no-stream"
            ValueAVX valS00 = *(ValueAVX *)localSumPtr(0, 0);
#pragma ss stream_name "gfm.conv3d_zxy_outer_tile.local.ld01/no-stream"
            ValueAVX valS01 = *(ValueAVX *)localSumPtr(0, 1);
#pragma ss stream_name "gfm.conv3d_zxy_outer_tile.local.ld02/no-stream"
            ValueAVX valS02 = *(ValueAVX *)localSumPtr(0, 2);
#pragma ss stream_name "gfm.conv3d_zxy_outer_tile.local.ld10/no-stream"
            ValueAVX valS10 = *(ValueAVX *)localSumPtr(1, 0);
#pragma ss stream_name "gfm.conv3d_zxy_outer_tile.local.ld11/no-stream"
            ValueAVX valS11 = *(ValueAVX *)localSumPtr(1, 1);
#pragma ss stream_name "gfm.conv3d_zxy_outer_tile.local.ld12/no-stream"
            ValueAVX valS12 = *(ValueAVX *)localSumPtr(1, 2);
#pragma ss stream_name "gfm.conv3d_zxy_outer_tile.local.ld20/no-stream"
            ValueAVX valS20 = *(ValueAVX *)localSumPtr(2, 0);
#pragma ss stream_name "gfm.conv3d_zxy_outer_tile.local.ld21/no-stream"
            ValueAVX valS21 = *(ValueAVX *)localSumPtr(2, 1);
#pragma ss stream_name "gfm.conv3d_zxy_outer_tile.local.ld22/no-stream"
            ValueAVX valS22 = *(ValueAVX *)localSumPtr(2, 2);

            for (int64_t i = 0; i < Ni; i += vElem) {

              int64_t idxI = (yy + y) * Nx * Ni + (xx + x) * Ni + i;

              ValueAVX valI = ValueAVXLoad(I + idxI);

              int64_t idxK = o * Kx * Ky * Ni + i;

              ValueAVX valK00 = ValueAVXLoad(K + idxK + 0 * Ni);
              ValueAVX valK01 = ValueAVXLoad(K + idxK + 1 * Ni);
              ValueAVX valK02 = ValueAVXLoad(K + idxK + 2 * Ni);
              ValueAVX valK10 = ValueAVXLoad(K + idxK + Kx * Ni + 0 * Ni);
              ValueAVX valK11 = ValueAVXLoad(K + idxK + Kx * Ni + 1 * Ni);
              ValueAVX valK12 = ValueAVXLoad(K + idxK + Kx * Ni + 2 * Ni);
              ValueAVX valK20 = ValueAVXLoad(K + idxK + 2 * Kx * Ni + 0 * Ni);
              ValueAVX valK21 = ValueAVXLoad(K + idxK + 2 * Kx * Ni + 1 * Ni);
              ValueAVX valK22 = ValueAVXLoad(K + idxK + 2 * Kx * Ni + 2 * Ni);

              ValueAVX valM00 = ValueAVXMul(valK00, valI);
              ValueAVX valM01 = ValueAVXMul(valK01, valI);
              ValueAVX valM02 = ValueAVXMul(valK02, valI);
              ValueAVX valM10 = ValueAVXMul(valK10, valI);
              ValueAVX valM11 = ValueAVXMul(valK11, valI);
              ValueAVX valM12 = ValueAVXMul(valK12, valI);
              ValueAVX valM20 = ValueAVXMul(valK20, valI);
              ValueAVX valM21 = ValueAVXMul(valK21, valI);
              ValueAVX valM22 = ValueAVXMul(valK22, valI);

              valS00 = ValueAVXAdd(valM00, valS00);
              valS01 = ValueAVXAdd(valM01, valS01);
              valS02 = ValueAVXAdd(valM02, valS02);
              valS10 = ValueAVXAdd(valM10, valS10);
              valS11 = ValueAVXAdd(valM11, valS11);
              valS12 = ValueAVXAdd(valM12, valS12);
              valS20 = ValueAVXAdd(valM20, valS20);
              valS21 = ValueAVXAdd(valM21, valS21);
              valS22 = ValueAVXAdd(valM22, valS22);
            }
#pragma ss stream_name "gfm.conv3d_zxy_outer_tile.local.st00/no-stream"
            *(ValueAVX *)localSumPtr(0, 0) = (ValueAVX)valS00;
#pragma ss stream_name "gfm.conv3d_zxy_outer_tile.local.st01/no-stream"
            *(ValueAVX *)localSumPtr(0, 1) = (ValueAVX)valS01;
#pragma ss stream_name "gfm.conv3d_zxy_outer_tile.local.st02/no-stream"
            *(ValueAVX *)localSumPtr(0, 2) = (ValueAVX)valS02;
#pragma ss stream_name "gfm.conv3d_zxy_outer_tile.local.st10/no-stream"
            *(ValueAVX *)localSumPtr(1, 0) = (ValueAVX)valS10;
#pragma ss stream_name "gfm.conv3d_zxy_outer_tile.local.st11/no-stream"
            *(ValueAVX *)localSumPtr(1, 1) = (ValueAVX)valS11;
#pragma ss stream_name "gfm.conv3d_zxy_outer_tile.local.st12/no-stream"
            *(ValueAVX *)localSumPtr(1, 2) = (ValueAVX)valS12;
#pragma ss stream_name "gfm.conv3d_zxy_outer_tile.local.st20/no-stream"
            *(ValueAVX *)localSumPtr(2, 0) = (ValueAVX)valS20;
#pragma ss stream_name "gfm.conv3d_zxy_outer_tile.local.st21/no-stream"
            *(ValueAVX *)localSumPtr(2, 1) = (ValueAVX)valS21;
#pragma ss stream_name "gfm.conv3d_zxy_outer_tile.local.st22/no-stream"
            *(ValueAVX *)localSumPtr(2, 2) = (ValueAVX)valS22;
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
            ValueAVX valS = ValueAVXLoad(localSum[y][x]);
            int64_t idxO = (yy + y - 1) * Nx * Nn + (xx + x - 1) * Nn + o;
            Value sum = ValueAVXReduceAdd(valS);
#pragma ss stream_name "gfm.conv3d_zxy_oyxi_outer_tile.o.st/nest=2"
            O[idxO] = sum;
          }
        }

#if LOOP_ORDER != LOOP_ORDER_BYBX_O_Y_X_I
      }
    }
  }
#else
    }
  }
#endif

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