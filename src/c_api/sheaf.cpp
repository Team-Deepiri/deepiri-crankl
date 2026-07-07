#include "crankl/sheaf.h"
#include "core/internal.hpp"

extern "C" {

double crankl_sheaf_resonance(const uint64_t *slots, size_t n, const uint64_t *other,
                               size_t n_other) {
    return crankl::sheaf_resonance(slots, n, other, n_other);
}

int crankl_sheaf_beta1_proxy(const uint64_t *slots, size_t n) {
    return crankl::sheaf_beta1_proxy(slots, n);
}

} // extern "C"
