#ifndef CRANKLE_PACK_H
#define CRANKLE_PACK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int crankle_pack_f32(const float *data, size_t count, uint64_t *out_slots, size_t n_slots,
                     float lambda, float mu);
int crankle_unpack_f32(const uint64_t *slots, size_t n_slots, float *out, size_t count);

#ifdef __cplusplus
}
#endif

#endif
