#ifndef CRANKLE_HOLONOMY_H
#define CRANKLE_HOLONOMY_H

#include "crankle/types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int crankle_holonomy(const crankle_cran_t *cran, const float *x, size_t dim, float *y);

#ifdef __cplusplus
}
#endif

#endif
