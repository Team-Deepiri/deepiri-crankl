#include "c_api/internal.hpp"
#include "crankl/holonomy.h"
#include "crankl_internal_api.hpp"

extern "C" {

int crankl_holonomy(const crankl_cran_t *cran, const float *x, size_t dim, float *y) {
    if (!cran || !x || !y || dim == 0)
        return CRANKL_ERR_NULL;
    int rc = crankl::holonomy::forward(cran, x, dim, y);
    return rc == 0 ? CRANKL_OK : CRANKL_ERR_INVALID;
}

} // extern "C"
