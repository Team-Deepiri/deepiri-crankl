#ifndef CRANKL_CRAN_H
#define CRANKL_CRAN_H

#include "crankl/types.h"

#ifdef __cplusplus
extern "C" {
#endif

int crankl_cran_write(const char *path, const crankl_cran_header_t *hdr, const uint64_t *slots,
                      const uint64_t *layer_stacks, const uint32_t *depths);
int crankl_cran_write_with_metadata(const char *path, const crankl_cran_header_t *hdr,
                                    const uint64_t *slots, const crankl_cran_metadata_t *meta);
/*
 * TODO(format-v3): add a full writer that accepts both optional history and
 * optional provenance, for example:
 *
 * int crankl_cran_write_full(const char *path,
 *                            const crankl_cran_header_t *hdr,
 *                            const uint64_t *slots,
 *                            const uint64_t *layer_stacks,
 *                            uint32_t n_stack_layers,
 *                            const crankl_cran_metadata_t *meta);
 *
 * Keep the two functions above as convenience/compatibility wrappers. Do not use
 * hdr->depth_max as an implicit stack count unless a stack pointer is supplied.
 */
/*
 * Ownership contract:
 *   - Each successful crankl_cran_read gives its handle an independent mapping, so any
 *     number of archives may be open at the same time.
 *   - crankl_cran_close is idempotent and is a no-op on a zero-initialised handle. It
 *     clears the handle's view, so a closed handle fails crankl_cran_verify.
 *   - crankl_cran_read does not inspect *out beforehand. Close a handle before reusing
 *     it for another archive, otherwise the previous mapping is leaked.
 *   - crankl_cran_t is a copyable POD and a copy is a NON-OWNING view. Close exactly the
 *     handle that was passed to crankl_cran_read, never a copy of it.
 */
int crankl_cran_read(const char *path, crankl_cran_t *out);
void crankl_cran_close(crankl_cran_t *cran);
int crankl_cran_verify(const crankl_cran_t *cran);
int crankl_cran_read_metadata(const crankl_cran_t *cran, crankl_cran_metadata_t *out);

#ifdef __cplusplus
}
#endif

#endif
