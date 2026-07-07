#include "c_api/internal.hpp"
#include "crankle/holonomy.h"
#include "crankle_internal_api.hpp"

extern "C" {

int crankle_holonomy(const crankle_cran_t *cran, const float *x, size_t dim, float *y) {
    if (!cran || !x || !y || dim == 0)
        return CRANKLE_ERR_NULL;
    int rc = crankle::holonomy::forward(cran, x, dim, y);
    return rc == 0 ? CRANKLE_OK : CRANKLE_ERR_INVALID;
}

} // extern "C"
