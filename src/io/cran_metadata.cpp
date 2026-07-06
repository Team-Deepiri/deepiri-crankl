#include "io/cran_metadata.hpp"
#include "io/cran_format.hpp"
#include "crankle_internal_api.hpp"
#include "crankle/crankle.h"
#include "xxhash.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace crankle {
namespace io {

int write_cran_with_metadata(const char *path, const ::crankle_cran_header_t *hdr,
                             const uint64_t *slots, const CranMetadata *meta) {
    if (!path || !hdr || !slots)
        return -1;

    std::vector<uint8_t> payload;
    for (uint64_t i = 0; i < hdr->n_slots; ++i) {
        uint64_t crank = slots[i];
        payload.insert(payload.end(), reinterpret_cast<uint8_t *>(&crank),
                       reinterpret_cast<uint8_t *>(&crank) + 8);
    }

    if (meta) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "{\"model\":\"%s\",\"hash\":\"%s\"}", meta->model_name,
                      meta->source_hash);
        uint32_t magic = FOOTER_MAGIC;
        uint32_t json_len = static_cast<uint32_t>(std::strlen(buf));
        payload.insert(payload.end(), reinterpret_cast<uint8_t *>(&magic),
                       reinterpret_cast<uint8_t *>(&magic) + 4);
        payload.insert(payload.end(), reinterpret_cast<uint8_t *>(&json_len),
                       reinterpret_cast<uint8_t *>(&json_len) + 4);
        payload.insert(payload.end(), buf, buf + json_len);
    }

    CranHeaderDisk hd{};
    std::memcpy(hd.magic, CRAN_MAGIC, 5);
    hd.magic[5] = '\0';
    hd.version = meta ? 2 : 1;
    hd.n_slots = hdr->n_slots;
    hd.depth_max = hdr->depth_max;
    hd.gamma = hdr->gamma;
    hd.flags = hdr->flags | (meta ? 1u : 0u);
    hd.checksum = crankle_xxhash64(payload.data(), payload.size(), 0);

    FILE *f = std::fopen(path, "wb");
    if (!f)
        return -2;
    std::fwrite(&hd, 1, sizeof(hd), f);
    std::fwrite(payload.data(), 1, payload.size(), f);
    std::fclose(f);
    return 0;
}

int read_cran_metadata(const ::crankle_cran_t *cran, CranMetadata *out) {
    if (!cran || !out || !cran->mmap_base)
        return -1;
    if ((cran->header.flags & 1u) == 0) {
        std::strncpy(out->model_name, "", sizeof(out->model_name) - 1);
        std::strncpy(out->source_hash, "", sizeof(out->source_hash) - 1);
        return 1; // no metadata
    }
    const auto *base = static_cast<const uint8_t *>(cran->mmap_base);
    size_t sz = cran->mmap_size;
    if (sz < sizeof(CranHeaderDisk) + 8)
        return -2;
    uint32_t magic = 0;
    std::memcpy(&magic, base + sz - 8, 4);
    if (magic != FOOTER_MAGIC)
        return -3;
    uint32_t json_len = 0;
    std::memcpy(&json_len, base + sz - 4, 4);
    if (json_len >= sizeof(out->model_name))
        json_len = static_cast<uint32_t>(sizeof(out->model_name) - 1);
    if (sz < sizeof(CranHeaderDisk) + 8 + json_len)
        return -4;
    char buf[256] = {};
    std::memcpy(buf, base + sz - 8 - json_len, json_len);
    // minimal parse
    const char *m = std::strstr(buf, "\"model\":\"");
    if (m) {
        m += 9;
        std::sscanf(m, "%127[^\"]", out->model_name);
    }
    const char *h = std::strstr(buf, "\"hash\":\"");
    if (h) {
        h += 8;
        std::sscanf(h, "%63[^\"]", out->source_hash);
    }
    return 0;
}

} // namespace io
} // namespace crankle
