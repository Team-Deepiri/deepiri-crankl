#ifndef CRANKL_PACK_H
#define CRANKL_PACK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CRANKL_BLOCK_FLOATS 64
#define CRANKL_UNPACK_DECRANK 0
#define CRANKL_UNPACK_COEFFS 1

size_t crankl_pack_n_slots(size_t float_count);

int crankl_pack_f32(const float *data, size_t count, uint64_t *out_slots, size_t n_slots,
                     float lambda, float mu);
int crankl_unpack_f32(const uint64_t *slots, size_t n_slots, float *out, size_t count);
int crankl_unpack_f32_mode(const uint64_t *slots, size_t n_slots, float *out, size_t count,
                            int mode);

double crankl_decrank_frobenius_loss(uint64_t word, const float block64[64]);

#ifdef __cplusplus
}
#endif

#endif
