#include "crankl/crankl.h"
#include "io/cran_format.hpp"
#include "io/security_limits.hpp"
#include "xxhash.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace crankl {
namespace io {

int write_cran(const char *path, const ::crankl_cran_header_t *hdr, const uint64_t *slots,
               const uint64_t *layer_stacks, const uint32_t *depths) {
    (void)depths;
    if (!path || !hdr || !slots)
        return -1;
    if (hdr->n_slots == 0 || hdr->n_slots > CRANKL_MAX_SLOTS)
        return -3;
    FILE *f = std::fopen(path, "wb");
    if (!f)
        return -2;

    std::vector<uint8_t> payload;
    for (uint64_t i = 0; i < hdr->n_slots; ++i) {
        uint64_t crank = slots[i];
        payload.insert(payload.end(), reinterpret_cast<uint8_t *>(&crank),
                       reinterpret_cast<uint8_t *>(&crank) + 8);
    }

    uint32_t n_stack_layers = 0;
    if (layer_stacks) {
        n_stack_layers = hdr->depth_max;
        if (n_stack_layers > CRANKL_MAX_STACK_LAYERS)
            return -4;
        uint64_t stack_word_count = 0;
        if (size_mul_overflow(n_stack_layers, hdr->n_slots, stack_word_count))
            return -5;
        payload.insert(payload.end(), reinterpret_cast<uint8_t *>(&n_stack_layers),
                       reinterpret_cast<uint8_t *>(&n_stack_layers) + 4);
        for (size_t i = 0; i < static_cast<size_t>(stack_word_count); ++i) {
            uint64_t w = layer_stacks[i];
            payload.insert(payload.end(), reinterpret_cast<uint8_t *>(&w),
                           reinterpret_cast<uint8_t *>(&w) + 8);
        }
    }

    CranHeaderDisk hd{};
    std::memcpy(hd.magic, CRANK_MAGIC, 6);
    hd.version = layer_stacks ? 2 : 1;
    hd.n_slots = hdr->n_slots;
    hd.depth_max = hdr->depth_max;
    hd.gamma = hdr->gamma;
    hd.flags = hdr->flags;
    hd.checksum = crankl_xxhash64(payload.data(), payload.size(), 0);

    std::fwrite(&hd, 1, sizeof(hd), f);
    std::fwrite(payload.data(), 1, payload.size(), f);
    std::fclose(f);
    return 0;
}

} // namespace io
} // namespace crankl
