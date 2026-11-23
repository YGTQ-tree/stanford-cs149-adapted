// Program 4 extra credit: hand-written AVX2 version of sqrt.
//
// Implement the same Newton iteration as sqrtSerial() / sqrt.ispc using AVX2
// intrinsics (8 floats per instruction). To get credit it should be about as
// fast as (or faster than) the ISPC version WITHOUT tasks.
//
// Hints:
//   * Each lane converges after a different number of iterations. Keep a
//     mask of lanes that are still "active" and loop while any lane is
//     active (_mm256_movemask_ps(mask) != 0).
//   * |x| can be computed with _mm256_andnot_ps(_mm256_set1_ps(-0.f), x).
//   * Only update `guess` in active lanes (_mm256_blendv_ps).
//   * Handle N % 8 != 0.
//
// The Makefile builds this file with -march=native, so AVX2 and FMA are
// available on both target machines. Be aware that FMA changes rounding, so
// the iteration count per element can differ slightly from the serial code.

#include <immintrin.h>
#include <math.h>

extern void sqrtSerial(int N, float initialGuess, float values[], float output[]);

void sqrtAVX2(int N, float initialGuess, float values[], float output[]) {
    // TODO (extra credit): replace this call with your AVX2 implementation.
    sqrtSerial(N, initialGuess, values, output);
}
