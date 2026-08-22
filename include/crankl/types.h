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
    /*
     * NULL means "not open": either never read, or already closed. It is never
     * MAP_FAILED -- that sentinel belongs to the reader's internal mapping owner
     * and is not part of this ABI. crankl_cran_verify and crankl_cran_read_metadata
     * both reject a handle on !mmap_base, so crankl_cran_close clears it to NULL
     * to make a closed handle fail those checks rather than pass them.
     *
     * The reader keeps no shared mutable state, so distinct handles are independent:
     * any number may be open at once, and reads into separate handles do not
     * interfere. No claim is made about concurrent use of the SAME handle.
     */
    void *mmap_base;
    size_t mmap_size;
    crankl_cran_header_t header;
    const uint64_t *slots;  /* n_slots current crank words */
    const uint64_t *layers; /* validated flattened stack snapshots, or NULL when
                             * the archive carries no history section. Callers
                             * must treat NULL as "nothing to peel from" rather
                             * than probing bytes after slots. */
    /*
     * TODO(format-v3): append an explicit uint32_t n_stack_layers field.
     * `layers` must be NULL when no validated stack section exists. The current
     * API has no reliable way to distinguish "no history" from a pointer to bytes
     * immediately after slots, which is unsafe when those bytes are metadata.
     */
} crankl_cran_t;

typedef struct crankl_cran_metadata {
    /* Provenance only: these strings describe origin; they are not weight history. */
    char model_name[128];
    char source_hash[64];
} crankl_cran_metadata_t;

#ifdef __cplusplus
}
#endif

#endif
