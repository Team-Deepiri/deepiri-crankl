#ifndef CRANKLE_TYPES_H
#define CRANKLE_TYPES_H

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

typedef struct crankle_cran_metadata {
    char model_name[128];
    char source_hash[64];
} crankle_cran_metadata_t;

#ifdef __cplusplus
}
#endif

#endif
