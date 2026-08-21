#include "internal_headers/simd.hpp"

#include <cstring>

#if defined(__AVX2__)
#include <immintrin.h>
#define CRANKL_AVX2_COMPILE 1
#else
#define CRANKL_AVX2_COMPILE 0
#endif

namespace crankl {
namespace simd {

#if CRANKL_AVX2_COMPILE
static inline double hsum256(__m256d s) {
    __m128d h = _mm_add_pd(_mm256_castpd256_pd128(s), _mm256_extractf128_pd(s, 1));
    h = _mm_add_sd(h, _mm_unpackhi_pd(h, h));
    return _mm_cvtsd_f64(h);
}
#endif

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

// y[v][r] = sum_c a[r*8+c] * x[v][c] — one matrix row against four consecutive
// [8]-wide state vectors at a time. The two row loads are shared across the
// four accumulators, amortizing row traffic across the batch.
void mat8_vec_batch_avx2(const double *a, const double *x, double *y, size_t batch) {
#if CRANKL_AVX2_COMPILE
    size_t v = 0;
    for (; v + 4 <= batch; v += 4) {
        const double *x0 = x + (v + 0) * 8;
        const double *x1 = x + (v + 1) * 8;
        const double *x2 = x + (v + 2) * 8;
        const double *x3 = x + (v + 3) * 8;
        for (int r = 0; r < 8; ++r) {
            const double *row = a + r * 8;
            __m256d lo = _mm256_loadu_pd(row);
            __m256d hi = _mm256_loadu_pd(row + 4);
            __m256d s0 = _mm256_mul_pd(lo, _mm256_loadu_pd(x0));
            __m256d s1 = _mm256_mul_pd(lo, _mm256_loadu_pd(x1));
            __m256d s2 = _mm256_mul_pd(lo, _mm256_loadu_pd(x2));
            __m256d s3 = _mm256_mul_pd(lo, _mm256_loadu_pd(x3));
            s0 = _mm256_fmadd_pd(hi, _mm256_loadu_pd(x0 + 4), s0);
            s1 = _mm256_fmadd_pd(hi, _mm256_loadu_pd(x1 + 4), s1);
            s2 = _mm256_fmadd_pd(hi, _mm256_loadu_pd(x2 + 4), s2);
            s3 = _mm256_fmadd_pd(hi, _mm256_loadu_pd(x3 + 4), s3);
            y[(v + 0) * 8 + r] = hsum256(s0);
            y[(v + 1) * 8 + r] = hsum256(s1);
            y[(v + 2) * 8 + r] = hsum256(s2);
            y[(v + 3) * 8 + r] = hsum256(s3);
        }
    }
    for (; v < batch; ++v) {
        for (int r = 0; r < 8; ++r) {
            double sum = 0.0;
            for (int c = 0; c < 8; ++c)
                sum += a[r * 8 + c] * x[v * 8 + c];
            y[v * 8 + r] = sum;
        }
    }
#else
    (void)a;
    (void)x;
    (void)y;
    (void)batch;
#endif
}

} // namespace simd
} // namespace crankl
