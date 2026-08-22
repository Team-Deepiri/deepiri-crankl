#ifndef CRANKL_HOLONOMY_H
#define CRANKL_HOLONOMY_H

#include "crankl/types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int crankl_holonomy(const crankl_cran_t *cran, const float *x, size_t dim, float *y);

/* Batched Wilson forward. x holds batch*dim floats (row-major vectors); y receives
 * batch*dim outputs. Numerically identical to calling crankl_holonomy per vector,
 * but the per-slot Padé exponentials are built once for the whole batch and the
 * matrix-vector applies use an AVX2 kernel when available. */
int crankl_holonomy_batch(const crankl_cran_t *cran, const float *x, size_t dim, size_t batch,
                          float *y);

/* Mean of crankl_holonomy over the batch: x/y_ref are [batch][dim]. */
double crankl_holonomy_mse_batch(const crankl_cran_t *cran, const float *x, const float *y_ref,
                                 size_t dim, size_t batch);

#ifdef __cplusplus
}
#endif

#endif
