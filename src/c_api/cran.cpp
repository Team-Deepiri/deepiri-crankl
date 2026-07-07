#include "c_api/internal.hpp"
#include "crankle/cran.h"
#include "crankle_internal_api.hpp"
#include "io/cran_metadata.hpp"

#include <cstring>

extern "C" {

int crankle_cran_write(const char *path, const crankle_cran_header_t *hdr, const uint64_t *slots,
                       const uint64_t *layer_stacks, const uint32_t *depths) {
    if (!path || !hdr || !slots)
        return CRANKLE_ERR_NULL;
    int rc = crankle::io::write_cran(path, hdr, slots, layer_stacks, depths);
    return rc == 0 ? CRANKLE_OK : CRANKLE_ERR_IO;
}

int crankle_cran_write_with_metadata(const char *path, const crankle_cran_header_t *hdr,
                                     const uint64_t *slots, const crankle_cran_metadata_t *meta) {
    if (!path || !hdr || !slots)
        return CRANKLE_ERR_NULL;
    crankle::io::CranMetadata m{};
    if (meta) {
        std::strncpy(m.model_name, meta->model_name, sizeof(m.model_name) - 1);
        std::strncpy(m.source_hash, meta->source_hash, sizeof(m.source_hash) - 1);
    }
    int rc = crankle::io::write_cran_with_metadata(path, hdr, slots, meta ? &m : nullptr);
    return rc == 0 ? CRANKLE_OK : CRANKLE_ERR_IO;
}

int crankle_cran_read(const char *path, crankle_cran_t *out) {
    if (!path || !out)
        return CRANKLE_ERR_NULL;
    int rc = crankle::io::read_cran(path, out);
    return rc == 0 ? CRANKLE_OK : CRANKLE_ERR_IO;
}

void crankle_cran_close(crankle_cran_t *cran) { crankle::io::close_cran(cran); }

int crankle_cran_verify(const crankle_cran_t *cran) {
    if (!cran)
        return CRANKLE_ERR_NULL;
    int rc = crankle::io::verify_cran(cran);
    return rc == 0 ? CRANKLE_OK : CRANKLE_ERR_FORMAT;
}

int crankle_cran_read_metadata(const crankle_cran_t *cran, crankle_cran_metadata_t *out) {
    if (!cran || !out)
        return CRANKLE_ERR_NULL;
    crankle::io::CranMetadata m{};
    int rc = crankle::io::read_cran_metadata(cran, &m);
    if (rc == 1)
        return CRANKLE_ERR_NO_METADATA;
    if (rc != 0)
        return CRANKLE_ERR_FORMAT;
    std::strncpy(out->model_name, m.model_name, sizeof(out->model_name) - 1);
    std::strncpy(out->source_hash, m.source_hash, sizeof(out->source_hash) - 1);
    return CRANKLE_OK;
}

} // extern "C"
