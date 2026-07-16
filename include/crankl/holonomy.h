#ifndef CRANKL_HOLONOMY_H
#define CRANKL_HOLONOMY_H

#include "crankl/types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int crankl_holonomy(const crankl_cran_t *cran, const float *x, size_t dim, float *y);

#ifdef __cplusplus
}
#endif

#endif
