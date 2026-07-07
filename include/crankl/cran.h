#ifndef CRANKL_CRAN_H
#define CRANKL_CRAN_H

#include "crankl/types.h"

#ifdef __cplusplus
extern "C" {
#endif

int crankl_cran_write(const char *path, const crankl_cran_header_t *hdr,
                       const uint64_t *slots, const uint64_t *layer_stacks, const uint32_t *depths);
int crankl_cran_write_with_metadata(const char *path, const crankl_cran_header_t *hdr,
                                     const uint64_t *slots, const crankl_cran_metadata_t *meta);
int crankl_cran_read(const char *path, crankl_cran_t *out);
void crankl_cran_close(crankl_cran_t *cran);
int crankl_cran_verify(const crankl_cran_t *cran);
int crankl_cran_read_metadata(const crankl_cran_t *cran, crankl_cran_metadata_t *out);

#ifdef __cplusplus
}
#endif

#endif
