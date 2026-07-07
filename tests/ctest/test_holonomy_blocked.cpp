#include "crankl/crankl.h"

#include <cmath>
#include <cstdio>
#include <vector>

int main() {
    const size_t dim = 16;
    const size_t n_slots = 2;

    std::vector<uint64_t> slots(n_slots);
    crankl_multivector_t mv{};
    mv.s = 0.1;
    mv.v[0] = 0.2;
    for (size_t s = 0; s < n_slots; ++s)
        slots[s] = crankl_crank_from_multivector(&mv, 1);

    crankl_cran_t cran{};
    cran.header.n_slots = n_slots;
    cran.header.gamma = 0.5f;
    cran.slots = slots.data();

    std::vector<float> x(dim), y(dim);
    for (size_t i = 0; i < dim; ++i)
        x[i] = static_cast<float>(i) * 0.05f;

    if (crankl_holonomy(&cran, x.data(), dim, y.data()) != 0)
        return 1;

    double energy = 0.0;
    for (size_t i = 0; i < dim; ++i)
        energy += static_cast<double>(y[i]) * y[i];
    if (!(energy > 0.0)) {
        std::fprintf(stderr, "FAIL: blocked holonomy produced zero output\n");
        return 2;
    }

    std::vector<float> y_target(dim);
    for (size_t i = 0; i < dim; ++i)
        y_target[i] = y[i] * 0.9f;

    double mse = crankl_holonomy_mse(&cran, x.data(), y_target.data(), dim);
    if (!(mse >= 0.0)) {
        std::fprintf(stderr, "FAIL: holonomy_mse invalid\n");
        return 3;
    }

    std::printf("test_holonomy_blocked ok energy=%g mse=%g\n", energy, mse);
    return 0;
}
