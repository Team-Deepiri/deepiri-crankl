#pragma once

#include "crankl/types.h"

#include <cstddef>

namespace crankl {
namespace holonomy {

int forward(const crankl_cran_t *cran, const float *x, size_t dim, float *y);
int forward_blocked(const crankl_cran_t *cran, const float *x, size_t dim, float *y);
double holonomy_mse(const crankl_cran_t *cran, const float *calib_x, const float *calib_y,
                    size_t dim);

} // namespace holonomy
} // namespace crankl
