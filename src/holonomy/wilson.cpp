#include "crankl/crankl.h"
#include "internal_headers/algebra.hpp"
#include "internal_headers/pack.hpp"
#include "internal_headers/holonomy.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace crankl {
namespace holonomy {

int forward_blocked(const ::crankl_cran_t *cran, const float *x, size_t dim, float *y);

static void apply_slot(const uint64_t *slot, float gamma, double state[8], size_t d) {
    std::array<double, 64> M{};
    decrank_matrix(*slot, M);
    double next[8] = {0};
    mat8_exp_i_apply(M.data(), static_cast<double>(gamma), state, next);
    for (size_t i = 0; i < d; ++i)
        state[i] = next[i];
}

int forward_blocked(const ::crankl_cran_t *cran, const float *x, size_t dim, float *y);

int forward(const ::crankl_cran_t *cran, const float *x, size_t dim, float *y) {
    if (!cran || !x || !y || dim == 0)
        return -1;

    float gamma = cran->header.gamma;
    if (gamma == 0.0f)
        gamma = 1.0f;

    if (dim <= pack::BLOCK_DIM) {
        size_t d = std::min(dim, pack::BLOCK_DIM);
        double state[8] = {0};
        for (size_t i = 0; i < d; ++i)
            state[i] = x[i];

        size_t n = static_cast<size_t>(cran->header.n_slots);
        for (size_t s = 0; s < n; ++s)
            apply_slot(&cran->slots[s], gamma, state, d);

        for (size_t i = 0; i < dim; ++i)
            y[i] = i < d ? static_cast<float>(state[i]) : 0.0f;
        return 0;
    }

    return forward_blocked(cran, x, dim, y);
}

int forward_blocked(const ::crankl_cran_t *cran, const float *x, size_t dim, float *y) {
    if (!cran || !x || !y || dim == 0)
        return -1;

    float gamma = cran->header.gamma;
    if (gamma == 0.0f)
        gamma = 1.0f;

    std::memset(y, 0, dim * sizeof(float));
    size_t n_blocks = (dim + pack::BLOCK_DIM - 1) / pack::BLOCK_DIM;
    size_t n_slots = static_cast<size_t>(cran->header.n_slots);

    for (size_t b = 0; b < n_blocks; ++b) {
        size_t off = b * pack::BLOCK_DIM;
        size_t d = std::min(pack::BLOCK_DIM, dim - off);
        double state[8] = {0};
        for (size_t i = 0; i < d; ++i)
            state[i] = x[off + i];

        if (b < n_slots) {
            apply_slot(&cran->slots[b], gamma, state, d);
        } else {
            size_t s = b % n_slots;
            apply_slot(&cran->slots[s], gamma, state, d);
        }

        for (size_t i = 0; i < d; ++i)
            y[off + i] = static_cast<float>(state[i]);
    }
    return 0;
}

double holonomy_mse(const ::crankl_cran_t *cran, const float *calib_x, const float *calib_y,
                    size_t dim) {
    if (!cran || !calib_x || !calib_y || dim == 0)
        return 0.0;

    std::vector<float> y(dim);
    if (dim > pack::BLOCK_DIM)
        forward_blocked(cran, calib_x, dim, y.data());
    else
        forward(cran, calib_x, dim, y.data());

    double mse = 0.0;
    for (size_t i = 0; i < dim; ++i) {
        double d = static_cast<double>(y[i] - calib_y[i]);
        mse += d * d;
    }
    return mse / static_cast<double>(dim);
}

} // namespace holonomy
} // namespace crankl
