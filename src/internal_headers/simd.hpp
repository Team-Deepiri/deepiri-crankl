#pragma once

#include <cstddef>
#include <cstdint>

namespace crankl {
namespace simd {

bool has_avx2();
void unpack_trits_batch(const uint64_t *words, size_t n, int *out_trits, size_t trits_per_word);
void mat8_mul_avx2(const double *a, const double *b, double *out);
void mat8_vec_batch_avx2(const double *a, const double *x, double *y, size_t batch);

} // namespace simd
} // namespace crankl
