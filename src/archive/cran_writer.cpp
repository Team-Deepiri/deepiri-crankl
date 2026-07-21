#include "crankl/crankl.h"
#include "internal_headers/archive.hpp"
#include "xxhash.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace crankl {
namespace io {

// LEGACY v1/v2 WRITER
// -------------------
// This function currently writes one of two layouts:
//   slots only:          [header][current slots]
//   slots plus history:  [header][current slots][layer count][stack words]
//
// A "layer" is a complete snapshot of all n_slots crank words. layer_stacks is
// flattened as layer_stacks[layer_index * n_slots + slot_index]. It is weight
// history used by peel; it is not metadata.
//
// This writer cannot append the provenance JSON produced by
// write_cran_with_metadata(), so callers must currently choose history OR metadata.
//
// TODO(format-v3): replace the two independent writer implementations with one
// internal writer accepting:
//   - required current slots,
//   - optional stack pointer plus an explicit n_stack_layers,
//   - optional metadata.
// Serialize in the fixed order slots -> stacks -> metadata, derive section flags
// from non-null options, hash the entire combined payload, and check every write.
// Keep this public API as a compatibility wrapper around that implementation.
//
// The depths parameter is currently ignored. Do not add it to the file until its
// intended semantics and length are specified; the v3 API should omit it unless a
// concrete per-slot depth use case is defined.
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
