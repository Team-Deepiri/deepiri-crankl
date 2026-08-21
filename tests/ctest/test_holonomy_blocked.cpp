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

    // Batched forward must agree with per-vector serial calls on the blocked path.
    const size_t batch = 7;
    std::vector<float> xb(batch * dim), yb(batch * dim), y1(dim);
    for (size_t v = 0; v < batch; ++v)
        for (size_t i = 0; i < dim; ++i)
            xb[v * dim + i] = static_cast<float>((v + 1) * (i + 1)) * 0.01f;
    if (crankl_holonomy_batch(&cran, xb.data(), dim, batch, yb.data()) != 0) {
        std::fprintf(stderr, "FAIL: crankl_holonomy_batch rejected\n");
        return 4;
    }
    for (size_t v = 0; v < batch; ++v) {
        if (crankl_holonomy(&cran, xb.data() + v * dim, dim, y1.data()) != 0) {
            std::fprintf(stderr, "FAIL: serial call %zu rejected\n", v);
            return 5;
        }
        for (size_t i = 0; i < dim; ++i) {
            float diff = yb[v * dim + i] - y1[i];
            if (std::fabs(diff) > 1e-4f) {
                std::fprintf(stderr, "FAIL: batch[%zu][%zu] got %g expected %g\n", v, i,
                             static_cast<double>(yb[v * dim + i]), static_cast<double>(y1[i]));
                return 6;
            }
        }
    }

    // Single-block path (dim <= 8): every slot applies in sequence.
    const size_t dim8 = 8;
    std::vector<float> x8(batch * dim8), y8(batch * dim8), y1_8(dim8);
    for (size_t v = 0; v < batch; ++v)
        for (size_t i = 0; i < dim8; ++i)
            x8[v * dim8 + i] = static_cast<float>(v + i + 1) * 0.05f;
    if (crankl_holonomy_batch(&cran, x8.data(), dim8, batch, y8.data()) != 0 ||
        crankl_holonomy(&cran, x8.data(), dim8, y1_8.data()) != 0) {
        std::fprintf(stderr, "FAIL: single-block calls rejected\n");
        return 7;
    }
    for (size_t v = 0; v < batch; ++v) {
        std::vector<float> one(dim8);
        if (v > 0 && crankl_holonomy(&cran, x8.data() + v * dim8, dim8, one.data()) != 0)
            return 8;
        const float *expect = v > 0 ? one.data() : y1_8.data();
        for (size_t i = 0; i < dim8; ++i) {
            if (std::fabs(y8[v * dim8 + i] - expect[i]) > 1e-4f) {
                std::fprintf(stderr, "FAIL: single-block batch[%zu][%zu]\n", v, i);
                return 9;
            }
        }
    }

    // Batched MSE equals the mean of per-vector MSEs.
    std::vector<float> target(batch * dim);
    for (size_t i = 0; i < batch * dim; ++i)
        target[i] = yb[i] * 0.95f;
    double mse_b = crankl_holonomy_mse_batch(&cran, xb.data(), target.data(), dim, batch);
    double mse_sum = 0.0;
    for (size_t v = 0; v < batch; ++v)
        mse_sum += crankl_holonomy_mse(&cran, xb.data() + v * dim, target.data() + v * dim, dim);
    if (std::fabs(mse_b - mse_sum / static_cast<double>(batch)) > 1e-9) {
        std::fprintf(stderr, "FAIL: mse_batch %g vs mean %g\n", mse_b,
                     mse_sum / static_cast<double>(batch));
        return 10;
    }

    // Null/zero guards.
    if (crankl_holonomy_batch(&cran, nullptr, dim, batch, yb.data()) == 0 ||
        crankl_holonomy_batch(&cran, xb.data(), 0, batch, yb.data()) == 0 ||
        crankl_holonomy_batch(&cran, xb.data(), dim, 0, yb.data()) == 0) {
        std::fprintf(stderr, "FAIL: batch guards missing\n");
        return 11;
    }

    std::printf("test_holonomy_blocked ok energy=%g mse=%g mse_batch=%g avx2=%d\n", energy, mse,
                mse_b, crankl_holonomy_avx2_supported());
    return 0;
}
