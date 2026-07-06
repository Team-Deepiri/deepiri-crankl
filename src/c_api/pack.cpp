#include "c_api/internal.hpp"
#include "crankle/pack.h"
#include "crankle_internal_api.hpp"

extern "C" {

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

} // extern "C"
