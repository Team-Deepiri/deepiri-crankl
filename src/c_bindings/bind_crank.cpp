#include "crankl/crank.h"
#include "internal_headers/c_bindings.hpp"

#include <cstring>

extern "C" {

uint64_t crankl_crank_from_multivector(const crankl_multivector_t *mv, uint8_t depth) {
    return crankl::pack_crank_word(crankl::capi::mv_from_c(mv), depth, 0);
}

void crankl_crank_to_multivector(uint64_t word, crankl_multivector_t *mv, uint8_t *depth_out) {
    crankl::Multivector m;
    uint8_t d = 0;
    crankl::unpack_crank_word(word, m, d);
    crankl::capi::mv_to_c(m, mv);
    if (depth_out)
        *depth_out = d;
}

void crankl_decrank_matrix(uint64_t word, double out8x8[64]) {
    if (!out8x8)
        return;
    std::array<double, 64> arr{};
    crankl::decrank_matrix(word, arr);
    std::memcpy(out8x8, arr.data(), 64 * sizeof(double));
}

} // extern "C"
