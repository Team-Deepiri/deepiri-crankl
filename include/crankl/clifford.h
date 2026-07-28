#ifndef CRANKL_CLIFFORD_H
#define CRANKL_CLIFFORD_H

#include "crankl/types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void crankl_clifford_reversion(const crankl_multivector_t *a, crankl_multivector_t *out);
void crankl_clifford_product(const crankl_multivector_t *a, const crankl_multivector_t *b,
                             crankl_multivector_t *out);
double crankl_clifford_resonance(uint64_t a, uint64_t b);

#ifdef __cplusplus
}
#endif

#endif
