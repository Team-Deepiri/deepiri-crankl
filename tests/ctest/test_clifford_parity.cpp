#include "crankl/crankl.h"

#include <cmath>
#include <cstdio>

#include "clifford_cases.hpp"

static uint64_t from_mv(const double m[8], uint8_t depth = 1) {
    crankl_multivector_t mv{};
    mv.s = m[0];
    for (int i = 0; i < 3; ++i) {
        mv.v[i] = m[1 + i];
        mv.b[i] = m[4 + i];
    }
    mv.p = m[7];
    return crankl_crank_from_multivector(&mv, depth);
}

int main() {
    int fails = 0;
    for (int i = 0; i < CLIFFORD_CASE_COUNT; ++i) {
        uint64_t wa = from_mv(CLIFFORD_CASES[i].a);
        uint64_t wb = from_mv(CLIFFORD_CASES[i].b);
        double r = crankl_clifford_resonance(wa, wb);
        if (std::fabs(r - CLIFFORD_CASES[i].expected) > CLIFFORD_CASES[i].tol) {
            std::fprintf(stderr, "FAIL case %d: got %g expected %g tol %g\n", i, r,
                         CLIFFORD_CASES[i].expected, CLIFFORD_CASES[i].tol);
            ++fails;
        }
    }

    crankl_multivector_t e1{0, {1, 0, 0}, {0, 0, 0}, 0};
    crankl_multivector_t e2{0, {0, 1, 0}, {0, 0, 0}, 0};
    crankl_multivector_t e12{0, {0, 0, 0}, {1, 0, 0}, 0};
    crankl_multivector_t prod{};

    crankl_clifford_product(&e1, &e1, &prod);
    if (std::fabs(prod.s - 1.0) > 1e-6) {
        std::fprintf(stderr, "FAIL e1*e1 scalar=%g\n", prod.s);
        ++fails;
    }
    crankl_clifford_product(&e1, &e2, &prod);
    if (std::fabs(prod.b[0] - 1.0) > 1e-6) {
        std::fprintf(stderr, "FAIL e1*e2 e12=%g\n", prod.b[0]);
        ++fails;
    }
    crankl_clifford_product(&e12, &e12, &prod);
    if (std::fabs(prod.s + 1.0) > 1e-6) {
        std::fprintf(stderr, "FAIL e12*e12 scalar=%g (want -1)\n", prod.s);
        ++fails;
    }

    if (fails == 0)
        std::printf("test_clifford_parity ok (%d cases)\n", CLIFFORD_CASE_COUNT);
    return fails;
}
