#include "c_api/internal.hpp"
#include "crankl/clifford.h"

extern "C" {

void crankl_clifford_reversion(const crankl_multivector_t *a, crankl_multivector_t *out) {
    crankl::Multivector mo{};
    crankl::clifford_reversion(crankl::capi::mv_from_c(a), mo);
    crankl::capi::mv_to_c(mo, out);
}

void crankl_clifford_product(const crankl_multivector_t *a, const crankl_multivector_t *b,
                              crankl_multivector_t *out) {
    crankl::Multivector mo{};
    crankl::clifford_product(crankl::capi::mv_from_c(a), crankl::capi::mv_from_c(b), mo);
    crankl::capi::mv_to_c(mo, out);
}

double crankl_clifford_resonance(uint64_t a, uint64_t b) {
    return crankl::clifford_resonance(a, b);
}

} // extern "C"
