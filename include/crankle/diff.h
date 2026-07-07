#ifndef CRANKLE_DIFF_H
#define CRANKLE_DIFF_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t crankle_crank_diff_count(const uint64_t *a, const uint64_t *b, size_t n);
double crankle_crank_diff_hamming(const uint64_t *a, const uint64_t *b, size_t n);

#ifdef __cplusplus
}
#endif

#endif
