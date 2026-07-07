#include "crankle/diff.h"
#include "core/internal.hpp"

extern "C" {

size_t crankle_crank_diff_count(const uint64_t *a, const uint64_t *b, size_t n) {
    return crankle::crank_diff_count(a, b, n);
}

double crankle_crank_diff_hamming(const uint64_t *a, const uint64_t *b, size_t n) {
    return crankle::crank_diff_hamming(a, b, n);
}

} // extern "C"
