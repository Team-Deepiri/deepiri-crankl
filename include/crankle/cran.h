#ifndef CRANKLE_CRAN_H
#define CRANKLE_CRAN_H

#include "crankle/types.h"

#ifdef __cplusplus
extern "C" {
#endif

int crankle_cran_write(const char *path, const crankle_cran_header_t *hdr,
                       const uint64_t *slots, const uint64_t *layer_stacks, const uint32_t *depths);
int crankle_cran_write_with_metadata(const char *path, const crankle_cran_header_t *hdr,
                                     const uint64_t *slots, const crankle_cran_metadata_t *meta);
int crankle_cran_read(const char *path, crankle_cran_t *out);
void crankle_cran_close(crankle_cran_t *cran);
int crankle_cran_verify(const crankle_cran_t *cran);
int crankle_cran_read_metadata(const crankle_cran_t *cran, crankle_cran_metadata_t *out);

#ifdef __cplusplus
}
#endif

#endif
