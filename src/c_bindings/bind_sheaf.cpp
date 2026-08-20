#include "crankl/sheaf.h"
#include "internal_headers/topology.hpp"

extern "C" {

double crankl_sheaf_resonance(const uint64_t *slots, size_t n, const uint64_t *other,
                              size_t n_other) {
    return crankl::sheaf_resonance(slots, n, other, n_other);
}

int crankl_sheaf_beta1_proxy(const uint64_t *slots, size_t n) {
    return crankl::sheaf_beta1_proxy(slots, n);
}

int crankl_sheaf_h0_dim(const uint64_t *slots, size_t n) {
    return crankl::sheaf_h0_dim(slots, n);
}

int crankl_sheaf_h1_dim(const uint64_t *slots, size_t n) {
    return crankl::sheaf_h1_dim(slots, n);
}

int crankl_sheaf_cohomology(const uint64_t *slots, size_t n, int *h0_out, int *h1_out) {
    return crankl::sheaf_cohomology(slots, n, h0_out, h1_out);
}

double crankl_sheaf_resonance_h1(const uint64_t *slots, size_t n, const uint64_t *other,
                                 size_t n_other) {
    return crankl::sheaf_resonance_h1(slots, n, other, n_other);
}

} // extern "C"
