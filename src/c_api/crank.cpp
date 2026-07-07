#include "c_api/internal.hpp"
#include "crankle/crank.h"

#include <cstring>

extern "C" {

uint64_t crankle_crank_from_multivector(const crankle_multivector_t *mv, uint8_t depth) {
    return crankle::pack_crank_word(crankle::capi::mv_from_c(mv), depth, 0);
}

void crankle_crank_to_multivector(uint64_t word, crankle_multivector_t *mv, uint8_t *depth_out) {
    crankle::Multivector m;
    uint8_t d = 0;
    crankle::unpack_crank_word(word, m, d);
    crankle::capi::mv_to_c(m, mv);
    if (depth_out)
        *depth_out = d;
}

void crankle_decrank_matrix(uint64_t word, double out8x8[64]) {
    if (!out8x8)
        return;
    std::array<double, 64> arr{};
    crankle::decrank_matrix(word, arr);
    std::memcpy(out8x8, arr.data(), 64 * sizeof(double));
}

} // extern "C"
