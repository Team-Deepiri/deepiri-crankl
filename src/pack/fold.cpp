#include "core/internal.hpp"
#include "pack/persistence.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace crankle {
namespace pack {

static double fold_objective(const Multivector &mv, const float *slice, size_t slice_len, float lambda,
                             float mu) {
    double err = 0.0;
    double vals[8] = {mv.s, mv.v[0], mv.v[1], mv.v[2], mv.b[0], mv.b[1], mv.b[2], mv.p};
    for (size_t i = 0; i < std::min(slice_len, size_t(8)); ++i) {
        double d = vals[i] - slice[i];
        err += d * d;
    }
    std::vector<float> sf(slice, slice + slice_len);
    auto diagram = persistence_diagram_1d(sf.data(), sf.size());
    std::vector<PersistencePair> zero;
    float w2 = wasserstein_persistence(diagram, zero);
    float pen = lambda * w2 + mu * spectral_range(sf.data(), sf.size());
    return err + static_cast<double>(pen) * 0.001;
}

static void perturb_mv(Multivector &mv, int seed) {
    double *parts[] = {&mv.s, &mv.v[0], &mv.v[1], &mv.v[2], &mv.b[0], &mv.b[1], &mv.b[2], &mv.p};
    int idx = seed % 8;
    parts[idx][0] += (seed % 2 == 0 ? 0.05 : -0.05);
}

int fold_f32(const float *data, size_t count, uint64_t *out_slots, size_t n_slots, float lambda,
             float mu) {
    if (!data || !out_slots || n_slots == 0)
        return -1;

    for (size_t s = 0; s < n_slots; ++s) {
        Multivector best{};
        size_t base = s * 8;
        if (base < count)
            best.s = data[base];
        for (int i = 0; i < 3 && base + 1 + i < count; ++i)
            best.v[i] = data[base + 1 + i];
        for (int i = 0; i < 3 && base + 4 + i < count; ++i)
            best.b[i] = data[base + 4 + i];
        if (base + 7 < count)
            best.p = data[base + 7];

        size_t slice_len = std::min<size_t>(8, count > base ? count - base : 0);
        double best_j = fold_objective(best, data + base, slice_len, lambda, mu);

        // Simulated annealing: 32 proposals per slot
        double temp = 1.0;
        for (int step = 0; step < 32; ++step) {
            Multivector trial = best;
            perturb_mv(trial, static_cast<int>(s * 32 + step));
            double j = fold_objective(trial, data + base, slice_len, lambda, mu);
            double delta = j - best_j;
            if (delta < 0.0 || std::exp(-delta / temp) > static_cast<double>(rand()) / RAND_MAX) {
                best = trial;
                best_j = j;
            }
            temp *= 0.92;
        }
        out_slots[s] = pack_crank_word(best, 1, 0);
    }
    return 0;
}

} // namespace pack
} // namespace crankle
