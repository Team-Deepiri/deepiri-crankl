#ifndef CRANKL_TYPES_H
#define CRANKL_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CRANKL_TRIT_ZERO 0
#define CRANKL_TRIT_PLUS 1
#define CRANKL_TRIT_MINUS 2

typedef struct crankl_crank {
    uint64_t word;
} crankl_crank_t;

typedef struct crankl_multivector {
    double s;
    double v[3];
    double b[3];
    double p;
} crankl_multivector_t;

typedef struct crankl_cran_header {
    uint64_t n_slots;
    uint32_t depth_max;
    float gamma;
    uint32_t flags;
} crankl_cran_header_t;

typedef struct crankl_cran {
    void *mmap_base;
    size_t mmap_size;
    crankl_cran_header_t header;
    const uint64_t *slots;
    const uint64_t *layers;
} crankl_cran_t;

typedef struct crankl_cran_metadata {
    char model_name[128];
    char source_hash[64];
} crankl_cran_metadata_t;

#ifdef __cplusplus
}
#endif

#endif
