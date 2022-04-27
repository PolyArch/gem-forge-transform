/**
 * XYZ means the data layout --
 *  Z (channel) is the outer-most dimension.
 *  X is the inner-most dimension,
 *
 * This version is optimized for PUM, in which we:
 * 1. Unroll Kx, Ky.
 */
#include "gfm_utils.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "immintrin.h"

typedef float Value;

#define NO_OPENMP

__attribute__((noinline)) Value foo(Value *I, Value *K, Value *O, int64_t Nx,
                                    int64_t Ny, int64_t Ni, int64_t Nn,
                                    int64_t Kx, int64_t Ky) {

  __builtin_assume(Nn > 0);

#pragma clang loop unroll(disable) interleave(disable) vectorize(disable)
  for (int64_t i = 0; i < Ni; ++i) {

#pragma clang loop unroll(disable) interleave(disable)
    for (int64_t o = 0; o < Nn; ++o) {

      // Move these assumption inside the loop so that OpenMP can capture it.
      __builtin_assume(Ny > 3);
      __builtin_assume(Nx > 3);
      __builtin_assume(Ni >= 16);
      __builtin_assume(Ni % 16 == 0);

      int64_t idxK = o * Kx * Ky * Ni + i;
#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.k00.ld/ld-cmp=0"
      Value valK00 = K[idxK + 0 * Ni];
#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.k01.ld/ld-cmp=0"
      Value valK01 = K[idxK + 1 * Ni];
#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.k02.ld/ld-cmp=0"
      Value valK02 = K[idxK + 2 * Ni];

#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.k10.ld/ld-cmp=1"
      Value valK10 = K[idxK + Kx * Ni + 0 * Ni];
#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.k11.ld/ld-cmp=1"
      Value valK11 = K[idxK + Kx * Ni + 1 * Ni];
#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.k12.ld/ld-cmp=1"
      Value valK12 = K[idxK + Kx * Ni + 2 * Ni];

#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.k20.ld/ld-cmp=2"
      Value valK20 = K[idxK + 2 * Kx * Ni + 0 * Ni];
#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.k21.ld/ld-cmp=2"
      Value valK21 = K[idxK + 2 * Kx * Ni + 1 * Ni];
#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.k22.ld/ld-cmp=2"
      Value valK22 = K[idxK + 2 * Kx * Ni + 2 * Ni];

#pragma clang loop unroll(disable) interleave(disable)
      for (int64_t y = 0; y < Ny - 2; ++y) {

#pragma clang loop unroll(disable) interleave(disable) vectorize(disable)
        for (int64_t x = 0; x < Nx - 2; ++x) {

          int64_t idxI = i * Nx * Ny + y * Nx + x;

#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.i00.ld/ld-cmp=0"
          Value valI00 = I[idxI + 0];
#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.i01.ld/ld-cmp=0/ld-cmp-result"
          Value valI01 = I[idxI + 1];
#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.i02.ld/ld-cmp=0"
          Value valI02 = I[idxI + 2];

          Value valI0 = valI00 * valK00 + valI01 * valK01 + valI02 * valK02;

#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.i10.ld/ld-cmp=1"
          Value valI10 = I[idxI + Nx + 0];
#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.i11.ld/ld-cmp=1/ld-cmp-result"
          Value valI11 = I[idxI + Nx + 1];
#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.i12.ld/ld-cmp=1"
          Value valI12 = I[idxI + Nx + 2];

          Value valI1 = valI10 * valK10 + valI11 * valK11 + valI12 * valK12;

#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.i20.ld/ld-cmp=2"
          Value valI20 = I[idxI + 2 * Nx + 0];
#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.i21.ld/ld-cmp=2/ld-cmp-result"
          Value valI21 = I[idxI + 2 * Nx + 1];
#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.i22.ld/ld-cmp=2"
          Value valI22 = I[idxI + 2 * Nx + 2];

          Value valI2 = valI20 * valK20 + valI21 * valK21 + valI22 * valK22;

          Value valI = valI0 + valI1 + valI2;

          int64_t idxO = o * Nx * Ny + y * Nx + x;

          // We write to (x + 1, y + 1, o)
#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.o.ld"
          Value valO = O[idxO + Nx + 1];

#pragma ss stream_name "gfm.conv3d_xyz_ioyx_outer.o.st"
          O[idxO + Nx + 1] = valI + valO;
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

#include "../conv3d_xyz_main.c"