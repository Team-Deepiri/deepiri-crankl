/* Minimal xxhash64 for Crankle checksums — subset for scaffold */
#pragma once
#include <cstddef>
#include <cstdint>

static inline uint64_t crankle_xxhash64(const void *input, size_t len, uint64_t seed) {
    const uint8_t *p = static_cast<const uint8_t *>(input);
    uint64_t h = seed + 0x9E3779B97F4A7C15ULL + len;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<uint64_t>(p[i]) << ((i % 8) * 8);
        h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ULL;
        h = (h ^ (h >> 27)) * 0x94D049BB133111EBULL;
        h = h ^ (h >> 31);
    }
    return h;
}
