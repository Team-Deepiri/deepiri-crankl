#include "crankl/crankl.h"

#include <cmath>
#include <cstdio>

int main() {
    crankl_multivector_t a{1.0, {1, 0, 0}, {0, 0, 0}, 0};
    crankl_multivector_t b{1.0, {0, 1, 0}, {0, 0, 0}, 0};
    uint64_t wa = crankl_crank_from_multivector(&a, 1);
    uint64_t wb = crankl_crank_from_multivector(&b, 1);
    double r = crankl_clifford_resonance(wa, wb);
    if (std::fabs(r) > 10.0) {
        std::printf("unexpected resonance %f\n", r);
        return 1;
    }
    std::printf("test_clifford ok resonance=%f\n", r);
    return 0;
}
