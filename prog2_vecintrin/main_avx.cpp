// Program 2, Part B: the same clampedExp problem, written with REAL x86
// SIMD intrinsics instead of the simulated CS149 intrinsics.
//
// Build:  make myexp_avx                                  (AVX2, both machines)
//         make myexp_avx AVXFLAGS="-mavx2 -mfma -mavx512f" (adds AVX-512, i5-11300H only)
// Run:    ./myexp_avx [-s N] [-r repeats]
//
// Auto-vectorization is disabled (-fno-tree-vectorize) so that the serial
// baseline really is scalar and the speedup you measure comes from your code.

#include <getopt.h>
#include <immintrin.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#include "CycleTimer.h"

#define EXP_MAX 10

// ---------------------------------------------------------------------------
// Serial reference (identical to main.cpp)
// ---------------------------------------------------------------------------
void clampedExpSerial(float* values, int* exponents, float* output, int N) {
  for (int i = 0; i < N; i++) {
    float x = values[i];
    int y = exponents[i];
    if (y == 0) {
      output[i] = 1.f;
    } else {
      float result = x;
      int count = y - 1;
      while (count > 0) {
        result *= x;
        count--;
      }
      if (result > 9.999999f) {
        result = 9.999999f;
      }
      output[i] = result;
    }
  }
}

float arraySumSerial(float* values, int N) {
  float sum = 0;
  for (int i = 0; i < N; i++) sum += values[i];
  return sum;
}

// ---------------------------------------------------------------------------
// Worked example: abs() with AVX2. Compare it with absVector() in main.cpp.
//
//   CS149 fake intrinsic            real AVX2 equivalent
//   ------------------------------  ------------------------------------------
//   __cs149_vec_float               __m256   (8 x float)
//   __cs149_vec_int                 __m256i  (8 x int32)
//   __cs149_mask                    __m256   with all-ones / all-zeros lanes
//                                   (AVX-512: a real mask register __mmask16)
//   _cs149_vload_float              _mm256_loadu_ps / _mm256_maskload_ps
//   _cs149_vstore_float             _mm256_storeu_ps / _mm256_maskstore_ps
//   _cs149_vlt_float                _mm256_cmp_ps(a, b, _CMP_LT_OQ)
//   _cs149_vsub_float               _mm256_sub_ps
//   masked "keep old value"         _mm256_blendv_ps(old, new, mask)
//   _cs149_cntbits                  __builtin_popcount(_mm256_movemask_ps(m))
//
// Unlike the CS149 library, most AVX2 arithmetic is NOT masked: every lane
// computes, and you use blendv to choose which result each lane keeps.
// ---------------------------------------------------------------------------
void absAVX2(float* values, float* output, int N) {
  const __m256 zero = _mm256_setzero_ps();
  int i = 0;
  for (; i + 8 <= N; i += 8) {
    __m256 x = _mm256_loadu_ps(values + i);
    __m256 isNeg = _mm256_cmp_ps(x, zero, _CMP_LT_OQ);  // if (x < 0)
    __m256 negX = _mm256_sub_ps(zero, x);               //   -x
    __m256 result = _mm256_blendv_ps(x, negX, isNeg);   // pick per lane
    _mm256_storeu_ps(output + i, result);
  }
  // Scalar tail for N % 8 leftover elements. (In the CS149 version you used a
  // partial mask instead; _mm256_maskstore_ps would also work here.)
  for (; i < N; i++) output[i] = fabsf(values[i]);
}

// ---------------------------------------------------------------------------
// TODO (required): vectorize clampedExpSerial with AVX2 (8 lanes).
//
// Requirements:
//   * Correct for ANY N (not only multiples of 8). Do not read or write past
//     output[N-1] in a way that changes memory the caller can observe.
//   * Use AVX2 intrinsics for the main loop. A scalar tail loop is allowed,
//     but try the masked approach (_mm256_maskload_ps / _mm256_maskstore_ps)
//     to mirror your CS149 solution.
//   * Useful: _mm256_loadu_si256, _mm256_cmpgt_epi32, _mm256_sub_epi32,
//     _mm256_castsi256_ps, _mm256_mul_ps, _mm256_min_ps, _mm256_blendv_ps,
//     _mm256_testz_si256 or _mm256_movemask_ps.
// ---------------------------------------------------------------------------
void clampedExpAVX2(float* values, int* exponents, float* output, int N) {
  // Placeholder so the program runs. Replace with your AVX2 implementation.
  clampedExpSerial(values, exponents, output, N);
}

// ---------------------------------------------------------------------------
// TODO (optional, i5-11300H only): vectorize with AVX-512 (16 lanes).
//
// AVX-512 has real mask registers (__mmask16) and masked arithmetic, e.g.
//   _mm512_mask_mul_ps(src, k, a, b)   -> lanes with k=0 keep src
//   _mm512_cmpgt_epi32_mask(a, b)      -> returns a __mmask16
//   _mm512_maskz_loadu_ps(k, ptr)      -> masked load, no fault on masked lanes
// This is the closest real-hardware match to the CS149 fake intrinsics.
// The Core Ultra 9 275HX does not support AVX-512; there this stays disabled.
// ---------------------------------------------------------------------------
#ifdef __AVX512F__
void clampedExpAVX512(float* values, int* exponents, float* output, int N) {
  // Placeholder. Replace with your AVX-512 implementation.
  clampedExpSerial(values, exponents, output, N);
}
#endif

// ---------------------------------------------------------------------------
// TODO (extra credit): vectorized array sum with AVX2.
// Aim for O(N/8 + log2(8)) work: accumulate in a __m256, then reduce the 8
// lanes with shuffles/hadd. Note that the result will differ slightly from
// the serial sum because floating-point addition is not associative.
// ---------------------------------------------------------------------------
float arraySumAVX2(float* values, int N) {
  // Placeholder. Replace with your AVX2 implementation.
  return arraySumSerial(values, N);
}
// ---------------------------------------------------------------------------
// Test harness (no need to modify)
// ---------------------------------------------------------------------------
static bool check(const char* name, float* out, float* gold, int N, int pad) {
  for (int i = 0; i < N + pad; i++) {
    if (fabsf(out[i] - gold[i]) > 1e-5f) {
      if (i >= N)
        printf("  [%s] wrote out of bounds at index %d!\n", name, i);
      else
        printf("  [%s] wrong value at [%d]: got %f expected %f\n", name, i, out[i], gold[i]);
      return false;
    }
  }
  return true;
}

template <typename F>
static double timeIt(F f, int repeats) {
  double best = 1e30;
  for (int r = 0; r < repeats; r++) {
    double t0 = CycleTimer::currentSeconds();
    f();
    best = std::min(best, CycleTimer::currentSeconds() - t0);
  }
  return best;
}

static void usage(const char* p) {
  printf("Usage: %s [options]\n", p);
  printf("  -s  --size <N>      workload size (default 1000003, not a multiple of 8)\n");
  printf("  -r  --repeats <R>   timing repeats, min is reported (default 5)\n");
  printf("  -?  --help          this message\n");
}

int main(int argc, char** argv) {
  int N = 1000003;
  int repeats = 5;
  static struct option long_options[] = {
      {"size", 1, 0, 's'}, {"repeats", 1, 0, 'r'}, {"help", 0, 0, '?'}, {0, 0, 0, 0}};
  int opt;
  while ((opt = getopt_long(argc, argv, "s:r:?", long_options, NULL)) != EOF) {
    switch (opt) {
      case 's': N = atoi(optarg); break;
      case 'r': repeats = atoi(optarg); break;
      default: usage(argv[0]); return 1;
    }
  }
  if (N <= 0 || repeats <= 0) { usage(argv[0]); return 1; }

  const int PAD = 16;  // guard area to catch out-of-bounds stores
  float* values = new float[N + PAD];
  int* exponents = new int[N + PAD];
  float* gold = new float[N + PAD];
  float* out = new float[N + PAD];

  srand(0);
  for (int i = 0; i < N + PAD; i++) {
    values[i] = -1.f + 4.f * static_cast<float>(rand()) / RAND_MAX;
    exponents[i] = rand() % EXP_MAX;
  }

  printf("N = %d\n\n", N);
  bool allOk = true;

  // abs example
  std::fill(gold, gold + N + PAD, 0.f);
  for (int i = 0; i < N; i++) gold[i] = fabsf(values[i]);
  std::fill(out, out + N + PAD, 0.f);
  absAVX2(values, out, N);
  printf("[abs AVX2 example]     %s\n", check("abs", out, gold, N, PAD) ? "correct" : "WRONG");

  // clampedExp
  std::fill(gold, gold + N + PAD, 0.f);
  double tSerial = timeIt([&] { clampedExpSerial(values, exponents, gold, N); }, repeats);
  printf("[clampedExp serial]    %8.3f ms\n", tSerial * 1000);

  std::fill(out, out + N + PAD, 0.f);
  double tAVX2 = timeIt([&] { clampedExpAVX2(values, exponents, out, N); }, repeats);
  bool ok = check("AVX2", out, gold, N, PAD);
  allOk &= ok;
  printf("[clampedExp AVX2]      %8.3f ms  (%.2fx)  %s\n", tAVX2 * 1000, tSerial / tAVX2,
         ok ? "correct" : "WRONG");

#ifdef __AVX512F__
  std::fill(out, out + N + PAD, 0.f);
  double tAVX512 = timeIt([&] { clampedExpAVX512(values, exponents, out, N); }, repeats);
  ok = check("AVX-512", out, gold, N, PAD);
  allOk &= ok;
  printf("[clampedExp AVX-512]   %8.3f ms  (%.2fx)  %s\n", tAVX512 * 1000, tSerial / tAVX512,
         ok ? "correct" : "WRONG");
#else
  printf("[clampedExp AVX-512]   not built (add -mavx512f to AVXFLAGS on a CPU with AVX-512)\n");
#endif

  // array sum (extra credit)
  float sumGold = 0.f, sumOut = 0.f;
  double tSumSerial = timeIt([&] { sumGold = arraySumSerial(values, N); }, repeats);
  double tSumAVX2 = timeIt([&] { sumOut = arraySumAVX2(values, N); }, repeats);
  bool sumOk = fabsf(sumGold - sumOut) <= 1e-3f * fabsf(sumGold) + 0.1f;
  printf("\n[arraySum serial]      %8.3f ms  = %f\n", tSumSerial * 1000, sumGold);
  printf("[arraySum AVX2]        %8.3f ms  = %f  (%.2fx)  %s\n", tSumAVX2 * 1000, sumOut,
         tSumSerial / tSumAVX2, sumOk ? "correct" : "WRONG");

  delete[] values;
  delete[] exponents;
  delete[] gold;
  delete[] out;
  printf("\n%s\n", allOk ? "Passed!!!" : "@@@ Failed!!!");
  return allOk ? 0 : 1;
}
