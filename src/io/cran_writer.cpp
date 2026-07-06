#include "crankle/crankle.h"
#include "io/cran_format.hpp"
#include "xxhash.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace crankle {
namespace io {

int write_cran(const char *path, const ::crankle_cran_header_t *hdr, const uint64_t *slots,
               const uint64_t *layer_stacks, const uint32_t *depths) {
    (void)layer_stacks;
    (void)depths;
    if (!path || !hdr || !slots)
        return -1;
    FILE *f = std::fopen(path, "wb");
    if (!f)
        return -2;

    std::vector<uint8_t> payload;
    for (uint64_t i = 0; i < hdr->n_slots; ++i) {
        uint64_t crank = slots[i];
        payload.insert(payload.end(), reinterpret_cast<uint8_t *>(&crank),
                       reinterpret_cast<uint8_t *>(&crank) + 8);
    }

    CranHeaderDisk hd{};
    std::memcpy(hd.magic, CRAN_MAGIC, 5);
    hd.magic[5] = '\0';
    hd.version = 1;
    hd.n_slots = hdr->n_slots;
    hd.depth_max = hdr->depth_max;
    hd.gamma = hdr->gamma;
    hd.flags = hdr->flags;
    hd.checksum = crankle_xxhash64(payload.data(), payload.size(), 0);

    std::fwrite(&hd, 1, sizeof(hd), f);
    std::fwrite(payload.data(), 1, payload.size(), f);
    std::fclose(f);
    return 0;
}

} // namespace io
} // namespace crankle
