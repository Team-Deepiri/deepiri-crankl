#ifndef CRANKL_TRIT_H
#define CRANKL_TRIT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int crankl_trit_encode(int trit, uint8_t *out2bits);
int crankl_trit_decode(uint8_t two_bits);

#ifdef __cplusplus
}
#endif

#endif
