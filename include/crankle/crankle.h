#ifndef CRANKLE_H
#define CRANKLE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CRANKLE_TRIT_ZERO 0
#define CRANKLE_TRIT_PLUS 1
#define CRANKLE_TRIT_MINUS 2

typedef struct crankle_crank {
    uint64_t word;
} crankle_crank_t;

typedef struct crankle_multivector {
    double s;
    double v[3];
    double b[3];
    double p;
} crankle_multivector_t;

typedef struct crankle_cran_header {
    uint64_t n_slots;
    uint32_t depth_max;
    float gamma;
    uint32_t flags;
} crankle_cran_header_t;

typedef struct crankle_cran {
    void *mmap_base;
    size_t mmap_size;
    crankle_cran_header_t header;
    const uint64_t *slots;
    const uint64_t *layers;
} crankle_cran_t;

int crankle_trit_encode(int trit, uint8_t *out2bits);
int crankle_trit_decode(uint8_t two_bits);

uint64_t crankle_crank_from_multivector(const crankle_multivector_t *mv, uint8_t depth);
void crankle_crank_to_multivector(uint64_t word, crankle_multivector_t *mv, uint8_t *depth_out);

void crankle_clifford_reversion(const crankle_multivector_t *a, crankle_multivector_t *out);
void crankle_clifford_product(const crankle_multivector_t *a, const crankle_multivector_t *b,
                              crankle_multivector_t *out);
double crankle_clifford_resonance(uint64_t a, uint64_t b);

void crankle_decrank_matrix(uint64_t word, double out8x8[64]);

double crankle_sheaf_resonance(const uint64_t *slots, size_t n, const uint64_t *other,
                               size_t n_other);
int crankle_sheaf_beta1_proxy(const uint64_t *slots, size_t n);

int crankle_turn(uint64_t *word, double lr);
int crankle_peel(uint64_t *word, uint32_t layers);
uint64_t crankle_bind(uint64_t a, uint64_t b);

int crankle_pack_f32(const float *data, size_t count, uint64_t *out_slots, size_t n_slots,
                     float lambda, float mu);
int crankle_unpack_f32(const uint64_t *slots, size_t n_slots, float *out, size_t count);

int crankle_cran_write(const char *path, const crankle_cran_header_t *hdr,
                       const uint64_t *slots, const uint64_t *layer_stacks, const uint32_t *depths);
int crankle_cran_read(const char *path, crankle_cran_t *out);
void crankle_cran_close(crankle_cran_t *cran);

int crankle_cran_verify(const crankle_cran_t *cran);
int crankle_holonomy(const crankle_cran_t *cran, const float *x, size_t dim, float *y);

#ifdef __cplusplus
}
#endif

#endif
