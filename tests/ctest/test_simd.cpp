#include "crankl/crankl.h"
#include "internal_headers/simd.hpp"

#include <cmath>
#include <cstdio>

// mat8_mul_avx2 picks its implementation at compile time from __AVX2__: intrinsics when -mavx2 is
// passed, a scalar triple loop otherwise. Architectures that never get -mavx2 (arm64, aarch64, ...)
// always take the fallback, so check whichever one we built against an independent reference.
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

    int fails = 0;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            double ref = 0.0;
            for (int k = 0; k < 8; ++k)
                ref += a[r * 8 + k] * b[k * 8 + c];
            // tolerance, not equality: the AVX2 path fuses multiply-add and rounds differently
            if (std::fabs(out[r * 8 + c] - ref) > 1e-9) {
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
    uint64_t words[2] = {0xFFFF0000FFFF0000ULL, 0x0000FFFF0000FFFFULL};
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
