#include "internal_headers/algebra.hpp"
#include "internal_headers/pack.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace crankl {
namespace pack {

namespace {

constexpr size_t kMaxProposals = 2000;
constexpr int kBladeCount = 7; // 3 vec + 3 bivec + 1 trivector
constexpr int kEditsPerBlade = 2;
constexpr size_t kMaxEdits = static_cast<size_t>(kBladeCount) * kEditsPerBlade;

struct TileState {
    std::array<double, BLOCK_FLOATS> block{};
    std::vector<PersistencePair> source_pd;
};

static int current_trit(double v) {
    if (v > 0.33)
        return TRIT_PLUS;
    if (v < -0.33)
        return TRIT_MINUS;
    return TRIT_ZERO;
}

static double dequantize(int t) {
    switch (t) {
    case TRIT_PLUS:
        return 1.0;
    case TRIT_MINUS:
        return -1.0;
    default:
        return 0.0;
    }
}

static void tile_joint(uint64_t word, const TileState &t, float alpha, float beta, double *w2_out,
                       double *frob_out) {
    std::array<double, BLOCK_FLOATS> M{};
    decrank_matrix(word, M);
    double frob = 0.0;
    std::vector<float> trial(BLOCK_FLOATS);
    for (size_t i = 0; i < BLOCK_FLOATS; ++i) {
        trial[i] = static_cast<float>(M[i]);
        double d = M[i] - t.block[i];
        frob += d * d;
    }
    auto trial_pd = persistence_diagram_1d(trial.data(), BLOCK_FLOATS);
    float w2 = wasserstein_persistence(t.source_pd, trial_pd);
    if (w2_out)
        *w2_out = static_cast<double>(w2);
    if (frob_out)
        *frob_out = frob;
}

static size_t collect_edits(uint64_t word, uint64_t candidates[kMaxEdits]) {
    Multivector mv;
    uint8_t depth;
    unpack_crank_word(word, mv, depth);
    uint8_t flags = static_cast<uint8_t>(word >> 60);
    double *blades[kBladeCount] = {&mv.vec[0],    &mv.vec[1],    &mv.vec[2],
                                   &mv.bivec[0],  &mv.bivec[1],  &mv.bivec[2],
                                   &mv.trivec};
    size_t n = 0;
    for (int b = 0; b < kBladeCount; ++b) {
        double saved = *blades[b];
        int t = current_trit(saved);
        for (int k = 1; k <= kEditsPerBlade; ++k) {
            int nt = (t + k) % 3;
            *blades[b] = dequantize(nt);
            candidates[n++] = pack_crank_word(mv, depth, flags);
        }
        *blades[b] = saved;
    }
    return n;
}

static uint32_t mix_seed(size_t s, unsigned seed) {
    uint32_t h = static_cast<uint32_t>(s) ^ (seed * 0x9E3779B1u);
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}

static int staged_anneal(const std::vector<TileState> &tiles, uint64_t *slots, float alpha,
                         float beta, unsigned seed, size_t budget) {
    std::vector<size_t> order(tiles.size());
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = i;

    size_t proposals = 0;
    bool improved = true;
    while (improved && proposals < budget) {
        improved = false;
        std::vector<double> frob(order.size());
        for (size_t s = 0; s < order.size(); ++s) {
            double w2, f;
            tile_joint(slots[s], tiles[s], alpha, beta, &w2, &f);
            frob[s] = f;
        }
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            if (frob[a] != frob[b])
                return frob[a] > frob[b];
            return mix_seed(a, seed) < mix_seed(b, seed);
        });
        for (size_t s : order) {
            if (proposals >= budget)
                break;
            ++proposals;
            double cur_w2, cur_frob;
            tile_joint(slots[s], tiles[s], alpha, beta, &cur_w2, &cur_frob);
            double cur_joint = alpha * cur_w2 + beta * cur_frob;
            uint64_t candidates[kMaxEdits];
            size_t n_edits = collect_edits(slots[s], candidates);
            uint64_t best_word = slots[s];
            double best_joint = cur_joint;
            for (size_t i = 0; i < n_edits; ++i) {
                double w2, f;
                tile_joint(candidates[i], tiles[s], alpha, beta, &w2, &f);
                double joint = alpha * w2 + beta * f;
                if (joint < best_joint) {
                    best_joint = joint;
                    best_word = candidates[i];
                }
            }
            if (best_word != slots[s]) {
                slots[s] = best_word;
                improved = true;
            }
        }
    }
    return 0;
}

static int bo_anneal(const std::vector<TileState> &tiles, uint64_t *slots, float alpha, float beta,
                     unsigned seed, size_t budget) {
    std::vector<double> mu(tiles.size());
    std::vector<size_t> visits(tiles.size(), 0);
    for (size_t s = 0; s < tiles.size(); ++s) {
        double w2, frob;
        tile_joint(slots[s], tiles[s], alpha, beta, &w2, &frob);
        mu[s] = alpha * w2 + beta * frob;
    }

    double explore = 0.3 + 0.4 * static_cast<double>(seed % 100) / 100.0;
    for (size_t t = 1; t <= budget; ++t) {
        size_t pick = 0;
        double best_acq = 1e300;
        for (size_t s = 0; s < tiles.size(); ++s) {
            double bonus =
                explore * std::sqrt(std::log(static_cast<double>(t) + 1.0) /
                                    static_cast<double>(visits[s] + 1));
            double acq = mu[s] - bonus;
            if (acq < best_acq) {
                best_acq = acq;
                pick = s;
            }
        }
        ++visits[pick];

        double cur_w2, cur_frob;
        tile_joint(slots[pick], tiles[pick], alpha, beta, &cur_w2, &cur_frob);
        double cur_joint = alpha * cur_w2 + beta * cur_frob;
        uint64_t candidates[kMaxEdits];
        size_t n_edits = collect_edits(slots[pick], candidates);
        uint64_t best_word = slots[pick];
        double best_joint = cur_joint;
        double obs_sum = cur_joint;
        size_t obs_n = 1;
        for (size_t i = 0; i < n_edits; ++i) {
            double w2, f;
            tile_joint(candidates[i], tiles[pick], alpha, beta, &w2, &f);
            double joint = alpha * w2 + beta * f;
            obs_sum += joint;
            ++obs_n;
            if (joint < best_joint) {
                best_joint = joint;
                best_word = candidates[i];
            }
        }
        if (best_word != slots[pick])
            slots[pick] = best_word;
        mu[pick] = obs_sum / static_cast<double>(obs_n);
    }
    return 0;
}

} // namespace

int fold_f32_anneal(const float *data, size_t count, uint64_t *out_slots, size_t n_slots,
                    float alpha, float beta, int mode, unsigned seed) {
    if (!data || !out_slots || n_slots == 0)
        return -1;
    if (mode == PACK_MODE_LEGACY)
        return fold_f32(data, count, out_slots, n_slots, alpha, beta);
    if (mode != PACK_MODE_STAGED && mode != PACK_MODE_BO)
        return -2;

    int rc = fold_f32(data, count, out_slots, n_slots, alpha, beta);
    if (rc != 0)
        return rc;

    std::vector<TileState> tiles(n_slots);
    for (size_t s = 0; s < n_slots; ++s) {
        float W[BLOCK_FLOATS];
        copy_weight_block(data, count, s, W);
        for (size_t i = 0; i < BLOCK_FLOATS; ++i)
            tiles[s].block[i] = static_cast<double>(W[i]);
        tiles[s].source_pd = persistence_diagram_1d(W, BLOCK_FLOATS);
    }

    size_t budget = std::min<size_t>(n_slots * 32, kMaxProposals);
    if (mode == PACK_MODE_STAGED)
        return staged_anneal(tiles, out_slots, alpha, beta, seed, budget);
    return bo_anneal(tiles, out_slots, alpha, beta, seed, budget);
}

double pack_objective(const float *data, size_t count, const uint64_t *slots, size_t n_slots,
                      double lambda, double *w2_out, double *frobenius_out) {
    double w2_total = 0.0;
    double frob_total = 0.0;
    for (size_t s = 0; s < n_slots; ++s) {
        float W[BLOCK_FLOATS];
        copy_weight_block(data, count, s, W);
        std::array<double, BLOCK_FLOATS> block{};
        for (size_t i = 0; i < BLOCK_FLOATS; ++i)
            block[i] = static_cast<double>(W[i]);
        std::array<double, BLOCK_FLOATS> M{};
        decrank_matrix(slots[s], M);
        double frob = 0.0;
        std::vector<float> trial(BLOCK_FLOATS);
        for (size_t i = 0; i < BLOCK_FLOATS; ++i) {
            trial[i] = static_cast<float>(M[i]);
            double d = M[i] - block[i];
            frob += d * d;
        }
        auto source_pd = persistence_diagram_1d(W, BLOCK_FLOATS);
        auto trial_pd = persistence_diagram_1d(trial.data(), BLOCK_FLOATS);
        w2_total += static_cast<double>(wasserstein_persistence(source_pd, trial_pd));
        frob_total += frob;
    }
    if (w2_out)
        *w2_out = w2_total;
    if (frobenius_out)
        *frobenius_out = frob_total;
    return lambda * w2_total + frob_total;
}

} // namespace pack
} // namespace crankl