#include "internal_headers/c_bindings.hpp"
#include "crankl/pack.h"
#include "internal_headers/dynamics.hpp"
#include "internal_headers/pack.hpp"

extern "C" {

size_t crankl_pack_n_slots(size_t float_count) {
    return crankl::pack::n_slots_from_count(float_count);
}

int crankl_pack_f32(const float *data, size_t count, uint64_t *out_slots, size_t n_slots,
                     float lambda, float mu) {
    if (!data || !out_slots || n_slots == 0)
        return CRANKL_ERR_NULL;
    return crankl::pack::fold_f32(data, count, out_slots, n_slots, lambda, mu);
}

int crankl_unpack_f32(const uint64_t *slots, size_t n_slots, float *out, size_t count) {
    if (!slots || !out)
        return CRANKL_ERR_NULL;
    return crankl::pack::unfold_f32(slots, n_slots, out, count);
}

int crankl_unpack_f32_mode(const uint64_t *slots, size_t n_slots, float *out, size_t count,
                            int mode) {
    if (!slots || !out)
        return CRANKL_ERR_NULL;
    return crankl::pack::unfold_f32_mode(slots, n_slots, out, count, mode);
}

double crankl_decrank_frobenius_loss(uint64_t word, const float block64[64]) {
    if (!block64)
        return 0.0;
    return crankl::decrank_frobenius_loss(word, block64);
}

} // extern "C"
