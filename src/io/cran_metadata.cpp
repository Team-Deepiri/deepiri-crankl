#include "io/cran_metadata.hpp"
#include "io/cran_format.hpp"
#include "crankl_internal_api.hpp"
#include "crankl/crankl.h"
#include "xxhash.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace crankl {
namespace io {

int write_cran_with_metadata(const char *path, const ::crankl_cran_header_t *hdr,
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
    hd.checksum = crankl_xxhash64(payload.data(), payload.size(), 0);

    FILE *f = std::fopen(path, "wb");
    if (!f)
        return -2;
    std::fwrite(&hd, 1, sizeof(hd), f);
    std::fwrite(payload.data(), 1, payload.size(), f);
    std::fclose(f);
    return 0;
}

int read_cran_metadata(const ::crankl_cran_t *cran, CranMetadata *out) {
    if (!cran || !out || !cran->mmap_base)
        return -1;
    if ((cran->header.flags & 1u) == 0) {
        out->model_name[0] = '\0';
        out->source_hash[0] = '\0';
        return 1; // no metadata
    }

    const auto *base = static_cast<const uint8_t *>(cran->mmap_base);
    const size_t payload_off = sizeof(CranHeaderDisk);
    const size_t slot_bytes = static_cast<size_t>(cran->header.n_slots) * 8;
    if (cran->mmap_size < payload_off + slot_bytes + 8)
        return -2;

    const uint8_t *footer = base + payload_off + slot_bytes;
    uint32_t magic = 0;
    std::memcpy(&magic, footer, 4);
    if (magic != FOOTER_MAGIC)
        return -3;

    uint32_t json_len = 0;
    std::memcpy(&json_len, footer + 4, 4);
    if (json_len >= 256)
        json_len = 255;
    if (cran->mmap_size < payload_off + slot_bytes + 8 + json_len)
        return -4;

    char buf[256] = {};
    std::memcpy(buf, footer + 8, json_len);

    out->model_name[0] = '\0';
    out->source_hash[0] = '\0';
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
} // namespace crankl
