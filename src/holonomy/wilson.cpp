#include "crankle/crankle.h"
#include "core/internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace crankle {
namespace holonomy {

int forward(const ::crankle_cran_t *cran, const float *x, size_t dim, float *y) {
    if (!cran || !x || !y || dim == 0)
        return -1;

    float gamma = cran->header.gamma;
    if (gamma == 0.0f)
        gamma = 1.0f;

    size_t d = std::min(dim, size_t(8));
    double state[8] = {0};
    for (size_t i = 0; i < d; ++i)
        state[i] = x[i];

    size_t n = static_cast<size_t>(cran->header.n_slots);
    for (size_t s = 0; s < n; ++s) {
        std::array<double, 64> M{};
        decrank_matrix(cran->slots[s], M);

        double next[8] = {0};
        mat8_exp_i_apply(M.data(), static_cast<double>(gamma), state, next);
        for (size_t i = 0; i < d; ++i)
            state[i] = next[i];
    }

    for (size_t i = 0; i < dim; ++i)
        y[i] = i < d ? static_cast<float>(state[i]) : 0.0f;
    return 0;
}

} // namespace holonomy
} // namespace crankle
