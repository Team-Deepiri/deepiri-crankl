#ifndef CRANKL_SHEAF_H
#define CRANKL_SHEAF_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

double crankl_sheaf_resonance(const uint64_t *slots, size_t n, const uint64_t *other,
                               size_t n_other);
int crankl_sheaf_beta1_proxy(const uint64_t *slots, size_t n);

#ifdef __cplusplus
}
#endif

#endif
