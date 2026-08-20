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

#define CRANKL_PACK_MODE_LEGACY 0
#define CRANKL_PACK_MODE_STAGED 1
#define CRANKL_PACK_MODE_BO 2

size_t crankl_pack_n_slots(size_t float_count);

int crankl_pack_f32(const float *data, size_t count, uint64_t *out_slots, size_t n_slots,
                    float lambda, float mu);
int crankl_pack_f32_anneal(const float *data, size_t count, uint64_t *out_slots, size_t n_slots,
                           float alpha, float beta, int mode, unsigned seed);
int crankl_pack_objective(const float *data, size_t count, const uint64_t *slots, size_t n_slots,
                          double lambda, double *w2_out, double *frobenius_out,
                          double *objective_out);
int crankl_pack_default_mode(void);
int crankl_unpack_f32(const uint64_t *slots, size_t n_slots, float *out, size_t count);
int crankl_unpack_f32_mode(const uint64_t *slots, size_t n_slots, float *out, size_t count,
                           int mode);

double crankl_decrank_frobenius_loss(uint64_t word, const float block64[64]);

#ifdef __cplusplus
}
#endif

#endif
