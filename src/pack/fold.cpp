#include "core/internal.hpp"
#include "pack/persistence.hpp"
#include "pack/tiling.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

namespace crankle {
namespace pack {

static void block_to_seed_mv(const float W[BLOCK_FLOATS], Multivector &mv) {
    mv = {};
    mv.s = W[0];
    for (int i = 0; i < 3; ++i)
        mv.v[i] = W[1 + i];
    for (int i = 0; i < 3; ++i)
        mv.b[i] = W[8 + i];
    mv.p = W[16];
}

static double decrank_frobenius_word(uint64_t word, const float W[BLOCK_FLOATS]) {
    std::array<double, 64> M{};
    decrank_matrix(word, M);
    double frob = 0.0;
    for (size_t i = 0; i < BLOCK_FLOATS; ++i) {
        double d = M[i] - static_cast<double>(W[i]);
        frob += d * d;
    }
    return frob;
}

static double fold_objective_decrank(const Multivector &mv, const float W[BLOCK_FLOATS], uint8_t depth,
                                     float lambda, float mu) {
    uint64_t word = pack_crank_word(mv, depth, 0);
    double frob = decrank_frobenius_word(word, W);

    std::vector<float> source(W, W + BLOCK_FLOATS);
    std::array<double, 64> M{};
    decrank_matrix(word, M);
    std::vector<float> trial(BLOCK_FLOATS);
    for (size_t i = 0; i < BLOCK_FLOATS; ++i)
        trial[i] = static_cast<float>(M[i]);

    auto source_pd = persistence_diagram_1d(source.data(), source.size());
    auto trial_pd = persistence_diagram_1d(trial.data(), trial.size());
    float w2 = wasserstein_persistence(source_pd, trial_pd);
    float pen = lambda * w2 + mu * spectral_range(source.data(), source.size());
    return frob + static_cast<double>(pen);
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
        float W[BLOCK_FLOATS];
        copy_weight_block(data, count, s, W);

        Multivector best{};
        block_to_seed_mv(W, best);
        uint8_t depth = 1;
        double best_j = fold_objective_decrank(best, W, depth, lambda, mu);

        std::mt19937 rng(static_cast<uint32_t>(0xC8411E00u ^ static_cast<uint32_t>(s)));
        std::uniform_real_distribution<double> coin(0.0, 1.0);

        double temp = 1.0;
        for (int step = 0; step < 128; ++step) {
            Multivector trial = best;
            perturb_mv(trial, static_cast<int>(s * 128 + step));
            double j = fold_objective_decrank(trial, W, depth, lambda, mu);
            double delta = j - best_j;
            if (delta < 0.0 || std::exp(-delta / temp) > coin(rng)) {
                best = trial;
                best_j = j;
            }
            temp *= 0.96;
        }
        out_slots[s] = pack_crank_word(best, depth, 0);
    }
    return 0;
}

} // namespace pack
} // namespace crankle
