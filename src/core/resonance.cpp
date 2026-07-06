#include "core/internal.hpp"

namespace crankle {

double clifford_resonance_pair(const uint64_t *a, const uint64_t *b, size_t n) {
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i)
        sum += clifford_resonance(a[i], b[i]);
    return n ? sum / static_cast<double>(n) : 0.0;
}

} // namespace crankle
