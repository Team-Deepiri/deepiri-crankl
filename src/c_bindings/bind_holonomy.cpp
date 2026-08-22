#include "crankl/holonomy.h"
#include "internal_headers/api.hpp"
#include "internal_headers/c_bindings.hpp"

extern "C" {

int crankl_holonomy(const crankl_cran_t *cran, const float *x, size_t dim, float *y) {
    if (!cran || !x || !y || dim == 0)
        return CRANKL_ERR_NULL;
    int rc = crankl::holonomy::forward(cran, x, dim, y);
    return rc == 0 ? CRANKL_OK : CRANKL_ERR_INVALID;
}

int crankl_holonomy_batch(const crankl_cran_t *cran, const float *x, size_t dim, size_t batch,
                          float *y) {
    if (!cran || !x || !y || dim == 0 || batch == 0)
        return CRANKL_ERR_NULL;
    int rc = crankl::holonomy::forward_batch(cran, x, dim, batch, y);
    return rc == 0 ? CRANKL_OK : CRANKL_ERR_INVALID;
}

double crankl_holonomy_mse_batch(const crankl_cran_t *cran, const float *x, const float *y_ref,
                                 size_t dim, size_t batch) {
    if (!cran || !x || !y_ref || dim == 0 || batch == 0)
        return 0.0;
    std::vector<float> out(static_cast<size_t>(batch) * dim);
    if (crankl::holonomy::forward_batch(cran, x, dim, batch, out.data()) != 0)
        return 0.0;
    double mse = 0.0;
    for (size_t v = 0; v < batch; ++v)
        mse += crankl::holonomy::holonomy_mse(cran, x + v * dim, y_ref + v * dim, dim);
    return mse / static_cast<double>(batch);
}

} // extern "C"
