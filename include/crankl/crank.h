#ifndef CRANKL_CRANK_H
#define CRANKL_CRANK_H

#include "crankl/types.h"

#ifdef __cplusplus
extern "C" {
#endif

uint64_t crankl_crank_from_multivector(const crankl_multivector_t *mv, uint8_t depth);
void crankl_crank_to_multivector(uint64_t word, crankl_multivector_t *mv, uint8_t *depth_out);
void crankl_decrank_matrix(uint64_t word, double out8x8[64]);

#ifdef __cplusplus
}
#endif

#endif
