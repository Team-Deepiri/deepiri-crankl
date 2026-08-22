#include "crankl/crankl.h"
#include "internal_headers/algebra.hpp"
#include "internal_headers/simd.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

// mat8_mul_avx2 picks its implementation at compile time from __AVX2__: intrinsics when -mavx2 is
// passed, a scalar triple loop otherwise. This checks whichever one was built against a reference
// loop written here.
//
// Note on what that is worth per target. On x86_64 the reference is genuinely independent: the
// implementation is _mm256_fmadd_pd intrinsics and this is a scalar loop. On arm64 the built
// implementation is itself a scalar triple loop, so the two are the same algorithm and this only
// catches a typo made in one copy and not the other. The arm64 value is that the fallback still
// compiles and links after the SIMD flags stop being applied.
static int check_mat8_mul() {
    double a[64];
    double b[64];
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            // asymmetric on purpose so a transposed index shows up as a wrong product
            a[r * 8 + c] = 1.0 + static_cast<double>(r) - 0.25 * static_cast<double>(c);
            b[r * 8 + c] = 0.5 * static_cast<double>(r) + 2.0 * static_cast<double>(c);
        }
    }

    double out[64];
    crankl::simd::mat8_mul_avx2(a, b, out);

    // Also drive the dispatcher callers actually use. crankl::mat8_mul branches on
    // simd::has_avx2(), which is a compile-time constant: on a target that never gets
    // -mavx2 it takes its own scalar loop in linalg.cpp and never reaches the function
    // above. Checking only mat8_mul_avx2 would leave the arithmetic that really runs on
    // those targets uncovered.
    double dispatched[64];
    crankl::mat8_mul(a, b, dispatched);

    int fails = 0;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            double ref = 0.0;
            for (int k = 0; k < 8; ++k)
                ref += a[r * 8 + k] * b[k * 8 + c];
            // Exact equality is correct here: a[] are multiples of 0.25 and b[] of 0.5, so every
            // product is a multiple of 0.125 and every 8-term sum is exactly representable (max
            // magnitude 892.5, well inside 2^53). FMA and separate multiply-add agree bitwise on
            // this data, so a tolerance would be inert and would hide a real rounding regression.
            if (dispatched[r * 8 + c] != ref) {
                std::fprintf(stderr, "FAIL: mat8_mul[%d][%d] got %g expected %g\n", r, c,
                             dispatched[r * 8 + c], ref);
                ++fails;
            }
            if (out[r * 8 + c] != ref) {
                std::fprintf(stderr, "FAIL: mat8_mul_avx2[%d][%d] got %g expected %g\n", r, c,
                             out[r * 8 + c], ref);
                ++fails;
            }
        }
    }
    return fails;
}

// Batched matrix-vector apply must match per-vector mat8_vec exactly in structure;
// values are compared with a tight tolerance since the AVX2 kernel reorders adds.
static int check_mat8_vec_batch() {
    double a[64];
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
            a[r * 8 + c] = 1.0 / (1.0 + static_cast<double>(r) + static_cast<double>(c));

    const size_t batch = 11; // deliberately not a multiple of 4 to exercise the remainder path
    std::vector<double> x(batch * 8), y_batch(batch * 8, 0.0), y_ref(batch * 8, 0.0);
    for (size_t v = 0; v < batch; ++v)
        for (int c = 0; c < 8; ++c)
            x[v * 8 + c] = 0.125 * static_cast<double>((v * 8 + c) % 16) - 0.5;

    for (size_t v = 0; v < batch; ++v)
        crankl::mat8_vec(a, x.data() + v * 8, y_ref.data() + v * 8);
    crankl::mat8_vec_batch(a, x.data(), y_batch.data(), batch);

    int fails = 0;
    for (size_t i = 0; i < batch * 8; ++i) {
        if (std::fabs(y_batch[i] - y_ref[i]) > 1e-12) {
            std::fprintf(stderr, "FAIL: mat8_vec_batch[%zu] got %.17g expected %.17g\n", i,
                         y_batch[i], y_ref[i]);
            ++fails;
        }
    }
    return fails;
}

int main() {
    std::printf("avx2=%d\n", crankl_has_avx2());
    // smoke: library loads and simd probe works
    if (crankl_has_avx2() < 0) {
        return 1;
    }
    if (check_mat8_mul() != 0) {
        return 1;
    }
    if (check_mat8_vec_batch() != 0) {
        return 1;
    }
    if (crankl_holonomy_avx2_supported() != crankl_has_avx2()) {
        std::fprintf(stderr, "FAIL: holonomy avx2 probe disagrees with simd probe\n");
        return 1;
    }
    std::printf("test_simd ok\n");
    return 0;
}
