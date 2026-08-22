#include "crankl/cran.h"
#include "internal_headers/archive.hpp"
#include "internal_headers/c_bindings.hpp"

#include <cstring>

namespace {
template <size_t N> void copy_bounded(char (&dst)[N], const char *src) {
    std::memcpy(dst, src, strnlen(src, N));
    dst[N - 1] = '\0';
}
} // namespace

extern "C" {

int crankl_cran_write(const char *path, const crankl_cran_header_t *hdr, const uint64_t *slots,
                      const uint64_t *layer_stacks, const uint32_t *depths) {
    if (!path || !hdr || !slots)
        return CRANKL_ERR_NULL;
    int rc = crankl::io::write_cran(path, hdr, slots, layer_stacks, depths);
    return rc == 0 ? CRANKL_OK : CRANKL_ERR_IO;
}

int crankl_cran_write_with_metadata(const char *path, const crankl_cran_header_t *hdr,
                                    const uint64_t *slots, const crankl_cran_metadata_t *meta) {
    if (!path || !hdr || !slots)
        return CRANKL_ERR_NULL;
    crankl::io::CranMetadata m{};
    if (meta) {
        copy_bounded(m.model_name, meta->model_name);
        copy_bounded(m.source_hash, meta->source_hash);
    }
    int rc = crankl::io::write_cran_with_metadata(path, hdr, slots, meta ? &m : nullptr);
    return rc == 0 ? CRANKL_OK : CRANKL_ERR_IO;
}

int crankl_cran_read(const char *path, crankl_cran_t *out) {
    if (!path || !out)
        return CRANKL_ERR_NULL;
    int rc = crankl::io::read_cran(path, out);
    return rc == 0 ? CRANKL_OK : CRANKL_ERR_IO;
}

void crankl_cran_close(crankl_cran_t *cran) {
    crankl::io::close_cran(cran);
}

int crankl_cran_verify(const crankl_cran_t *cran) {
    if (!cran)
        return CRANKL_ERR_NULL;
    int rc = crankl::io::verify_cran(cran);
    return rc == 0 ? CRANKL_OK : CRANKL_ERR_FORMAT;
}

int crankl_cran_read_metadata(const crankl_cran_t *cran, crankl_cran_metadata_t *out) {
    if (!cran || !out)
        return CRANKL_ERR_NULL;
    crankl::io::CranMetadata m{};
    int rc = crankl::io::read_cran_metadata(cran, &m);
    if (rc == 1)
        return CRANKL_ERR_NO_METADATA;
    if (rc != 0)
        return CRANKL_ERR_FORMAT;
    copy_bounded(out->model_name, m.model_name);
    copy_bounded(out->source_hash, m.source_hash);
    return CRANKL_OK;
}

} // extern "C"
