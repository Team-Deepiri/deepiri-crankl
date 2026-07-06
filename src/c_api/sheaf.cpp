#include "crankle/sheaf.h"
#include "core/internal.hpp"

extern "C" {

double crankle_sheaf_resonance(const uint64_t *slots, size_t n, const uint64_t *other,
                               size_t n_other) {
    return crankle::sheaf_resonance(slots, n, other, n_other);
}

int crankle_sheaf_beta1_proxy(const uint64_t *slots, size_t n) {
    return crankle::sheaf_beta1_proxy(slots, n);
}

} // extern "C"
