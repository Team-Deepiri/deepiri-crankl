#ifndef CRANKLE_CRANK_H
#define CRANKLE_CRANK_H

#include "crankle/types.h"

#ifdef __cplusplus
extern "C" {
#endif

uint64_t crankle_crank_from_multivector(const crankle_multivector_t *mv, uint8_t depth);
void crankle_crank_to_multivector(uint64_t word, crankle_multivector_t *mv, uint8_t *depth_out);
void crankle_decrank_matrix(uint64_t word, double out8x8[64]);

#ifdef __cplusplus
}
#endif

#endif
