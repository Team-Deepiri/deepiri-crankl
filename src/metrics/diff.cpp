#include "internal_headers/metrics.hpp"

#include <cstddef>
#include <cstdint>

namespace crankl {

size_t crank_diff_count(const uint64_t *a, const uint64_t *b, size_t n) {
    size_t diff = 0;
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i])
            ++diff;
    }
    return diff;
}

double crank_diff_hamming(const uint64_t *a, const uint64_t *b, size_t n) {
    if (n == 0)
        return 0.0;
    size_t bits = 0;
    for (size_t i = 0; i < n; ++i) {
        uint64_t x = a[i] ^ b[i];
        while (x) {
            bits += x & 1;
            x >>= 1;
        }
    }
    return static_cast<double>(bits) / static_cast<double>(n * 64);
}

} // namespace crankl
