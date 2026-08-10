#include "crankl/crankl.h"
#include "internal_headers/algebra.hpp"
#include "internal_headers/simd.hpp"

#include <cmath>
#include <cstdio>

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

int main() {
    std::printf("avx2=%d\n", crankl_has_avx2());
    // smoke: library loads and simd probe works
    if (crankl_has_avx2() < 0) {
        return 1;
    }
    if (check_mat8_mul() != 0) {
        return 1;
    }
    std::printf("test_simd ok\n");
    return 0;
}
