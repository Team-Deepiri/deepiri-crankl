#include "core/simd.hpp"

#include <cstring>

#if defined(__AVX2__)
#include <immintrin.h>
#define CRANKL_AVX2_COMPILE 1
#else
#define CRANKL_AVX2_COMPILE 0
#endif

namespace crankl {
namespace simd {

bool has_avx2() {
#if CRANKL_AVX2_COMPILE
    return true;
#else
    return false;
#endif
}

void unpack_trits_batch(const uint64_t *words, size_t n, int *out_trits, size_t trits_per_word) {
    size_t out_idx = 0;
    for (size_t w = 0; w < n; ++w) {
        uint64_t word = words[w];
        for (size_t t = 0; t < trits_per_word && t < 16; ++t) {
            int enc = static_cast<int>((word >> (16 + t * 2)) & 3);
            switch (enc) {
            case 1:
                out_trits[out_idx++] = 1;
                break;
            case 2:
                out_trits[out_idx++] = -1;
                break;
            default:
                out_trits[out_idx++] = 0;
                break;
            }
        }
    }
}

void mat8_mul_avx2(const double *a, const double *b, double *out) {
#if CRANKL_AVX2_COMPILE
    for (int r = 0; r < 8; ++r) {
        __m256d acc0 = _mm256_setzero_pd();
        __m256d acc1 = _mm256_setzero_pd();
        for (int k = 0; k < 8; ++k) {
            __m256d bk0 = _mm256_loadu_pd(&b[k * 8 + 0]);
            __m256d bk1 = _mm256_loadu_pd(&b[k * 8 + 4]);
            __m256d ar = _mm256_set1_pd(a[r * 8 + k]);
            acc0 = _mm256_fmadd_pd(ar, bk0, acc0);
            acc1 = _mm256_fmadd_pd(ar, bk1, acc1);
        }
        _mm256_storeu_pd(&out[r * 8 + 0], acc0);
        _mm256_storeu_pd(&out[r * 8 + 4], acc1);
    }
#else
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            double sum = 0.0;
            for (int k = 0; k < 8; ++k)
                sum += a[r * 8 + k] * b[k * 8 + c];
            out[r * 8 + c] = sum;
        }
    }
#endif
}

} // namespace simd
} // namespace crankl
