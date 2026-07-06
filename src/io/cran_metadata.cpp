#include "io/cran_metadata.hpp"
#include "io/cran_format.hpp"
#include "crankle_internal_api.hpp"
#include "crankle/crankle.h"
#include "xxhash.h"

#include <cstdio>
#include <cstring>

namespace crankle {
namespace io {

int write_cran(const char *path, const ::crankle_cran_header_t *hdr, const uint64_t *slots,
               const uint64_t *layer_stacks, const uint32_t *depths);

static constexpr uint32_t FOOTER_MAGIC = 0x4D455441u; // META

int write_cran_with_metadata(const char *path, const ::crankle_cran_header_t *hdr,
                             const uint64_t *slots, const CranMetadata *meta) {
    if (write_cran(path, hdr, slots, nullptr, nullptr) != 0)
        return -1;
    if (!meta)
        return 0;
    FILE *f = std::fopen(path, "ab");
    if (!f)
        return -2;
    uint32_t magic = FOOTER_MAGIC;
    uint32_t json_len = static_cast<uint32_t>(std::strlen(meta->model_name) + std::strlen(meta->source_hash) + 32);
    std::fwrite(&magic, 4, 1, f);
    std::fwrite(&json_len, 4, 1, f);
    char buf[256];
    std::snprintf(buf, sizeof(buf), "{\"model\":\"%s\",\"hash\":\"%s\"}", meta->model_name,
                  meta->source_hash);
    std::fwrite(buf, 1, std::strlen(buf), f);
    std::fclose(f);
    return 0;
}

int read_cran_metadata(const ::crankle_cran_t *cran, CranMetadata *out) {
    if (!cran || !out || !cran->mmap_base)
        return -1;
    const auto *base = static_cast<const uint8_t *>(cran->mmap_base);
    size_t sz = cran->mmap_size;
    if (sz < 8)
        return -2;
    uint32_t magic = 0;
    std::memcpy(&magic, base + sz - 8, 4);
    if (magic != FOOTER_MAGIC)
        return -3;
    std::strncpy(out->model_name, "unknown", sizeof(out->model_name) - 1);
    std::strncpy(out->source_hash, "", sizeof(out->source_hash) - 1);
    return 0;
}

} // namespace io
} // namespace crankle
