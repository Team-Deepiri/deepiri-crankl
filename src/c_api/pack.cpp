#include "c_api/internal.hpp"
#include "crankle/pack.h"
#include "crankle_internal_api.hpp"
#include "pack/tiling.hpp"

extern "C" {

size_t crankle_pack_n_slots(size_t float_count) {
    return crankle::pack::n_slots_from_count(float_count);
}

int crankle_pack_f32(const float *data, size_t count, uint64_t *out_slots, size_t n_slots,
                     float lambda, float mu) {
    if (!data || !out_slots || n_slots == 0)
        return CRANKLE_ERR_NULL;
    return crankle::pack::fold_f32(data, count, out_slots, n_slots, lambda, mu);
}

int crankle_unpack_f32(const uint64_t *slots, size_t n_slots, float *out, size_t count) {
    if (!slots || !out)
        return CRANKLE_ERR_NULL;
    return crankle::pack::unfold_f32(slots, n_slots, out, count);
}

int crankle_unpack_f32_mode(const uint64_t *slots, size_t n_slots, float *out, size_t count,
                            int mode) {
    if (!slots || !out)
        return CRANKLE_ERR_NULL;
    return crankle::pack::unfold_f32_mode(slots, n_slots, out, count, mode);
}

double crankle_decrank_frobenius_loss(uint64_t word, const float block64[64]) {
    if (!block64)
        return 0.0;
    return crankle::decrank_frobenius_loss(word, block64);
}

} // extern "C"
