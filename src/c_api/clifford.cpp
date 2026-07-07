#include "c_api/internal.hpp"
#include "crankle/clifford.h"

extern "C" {

void crankle_clifford_reversion(const crankle_multivector_t *a, crankle_multivector_t *out) {
    crankle::Multivector mo{};
    crankle::clifford_reversion(crankle::capi::mv_from_c(a), mo);
    crankle::capi::mv_to_c(mo, out);
}

void crankle_clifford_product(const crankle_multivector_t *a, const crankle_multivector_t *b,
                              crankle_multivector_t *out) {
    crankle::Multivector mo{};
    crankle::clifford_product(crankle::capi::mv_from_c(a), crankle::capi::mv_from_c(b), mo);
    crankle::capi::mv_to_c(mo, out);
}

double crankle_clifford_resonance(uint64_t a, uint64_t b) {
    return crankle::clifford_resonance(a, b);
}

} // extern "C"
