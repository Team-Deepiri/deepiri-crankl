#ifndef CRANKLE_CLIFFORD_H
#define CRANKLE_CLIFFORD_H

#include "crankle/types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void crankle_clifford_reversion(const crankle_multivector_t *a, crankle_multivector_t *out);
void crankle_clifford_product(const crankle_multivector_t *a, const crankle_multivector_t *b,
                              crankle_multivector_t *out);
double crankle_clifford_resonance(uint64_t a, uint64_t b);

#ifdef __cplusplus
}
#endif

#endif
