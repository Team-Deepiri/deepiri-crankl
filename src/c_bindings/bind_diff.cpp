#include "crankl/diff.h"
#include "internal_headers/metrics.hpp"

extern "C" {

size_t crankl_crank_diff_count(const uint64_t *a, const uint64_t *b, size_t n) {
    return crankl::crank_diff_count(a, b, n);
}

double crankl_crank_diff_hamming(const uint64_t *a, const uint64_t *b, size_t n) {
    return crankl::crank_diff_hamming(a, b, n);
}

} // extern "C"
