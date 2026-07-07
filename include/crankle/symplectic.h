#ifndef CRANKLE_SYMPLECTIC_H
#define CRANKLE_SYMPLECTIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int crankle_turn(uint64_t *word, double lr);
int crankle_peel(uint64_t *word, uint32_t layers);
uint64_t crankle_bind(uint64_t a, uint64_t b);

#ifdef __cplusplus
}
#endif

#endif
