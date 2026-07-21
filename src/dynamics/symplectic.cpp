#include "internal_headers/algebra.hpp"
#include "internal_headers/dynamics.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace crankl {

static double hamiltonian(const Multivector &mv) {
    double h = mv.scalar * mv.scalar;
    for (int i = 0; i < 3; ++i) {
        h += mv.vec[i] * mv.vec[i];
        h += mv.bivec[i] * mv.bivec[i];
    }
    h += mv.trivec * mv.trivec;
    return 0.5 * h;
}

static Multivector bch_exp_first_order(const Multivector &xi) {
    Multivector one{};
    one.scalar = 1.0;
    Multivector xi2{};
    clifford_product(xi, xi, xi2);
    Multivector out{};
    out.scalar = one.scalar + xi.scalar + 0.5 * xi2.scalar;
    for (int i = 0; i < 3; ++i) {
        out.vec[i] = xi.vec[i] + 0.5 * xi2.vec[i];
        out.bivec[i] = xi.bivec[i] + 0.5 * xi2.bivec[i];
    }
    out.trivec = xi.trivec + 0.5 * xi2.trivec;
    return out;
}

static void apply_trit_cycle(Multivector &mv, int field, int idx) {
    double *target = nullptr;
    if (field == 0)
        target = &mv.vec[idx];
    else if (field == 1)
        target = &mv.bivec[idx];
    else
        target = &mv.trivec;

    int t = TRIT_ZERO;
    if (*target > 0.33)
        t = TRIT_PLUS;
    else if (*target < -0.33)
        t = TRIT_MINUS;

    t = (t + 1) % 3;
    switch (t) {
    case TRIT_PLUS:
        *target = 1.0;
        break;
    case TRIT_MINUS:
        *target = -1.0;
        break;
    default:
        *target = 0.0;
        break;
    }
}

double decrank_frobenius_loss(uint64_t word, const float W[64]) {
    std::array<double, 64> M{};
    decrank_matrix(word, M);
    double frob = 0.0;
    for (int i = 0; i < 64; ++i) {
        double d = M[i] - static_cast<double>(W[i]);
        frob += d * d;
    }
    return frob;
}

int symplectic_turn(uint64_t &word, double lr) {
    Multivector mv;
    uint8_t depth;
    unpack_crank_word(word, mv, depth);
    double h0 = hamiltonian(mv);

    double p[3] = {mv.vec[0], mv.vec[1], mv.vec[2]};
    double q[3] = {mv.bivec[0], mv.bivec[1], mv.bivec[2]};

    for (int i = 0; i < 3; ++i)
        p[i] -= 0.5 * lr * q[i];
    for (int i = 0; i < 3; ++i)
        q[i] += lr * p[i];
    for (int i = 0; i < 3; ++i)
        p[i] -= 0.5 * lr * q[i];

    mv.vec[0] = p[0];
    mv.vec[1] = p[1];
    mv.vec[2] = p[2];
    mv.bivec[0] = q[0];
    mv.bivec[1] = q[1];
    mv.bivec[2] = q[2];

    Multivector xi{};
    xi.bivec[0] = lr * (p[1] * q[2] - p[2] * q[1]);
    xi.bivec[1] = lr * (p[2] * q[0] - p[0] * q[2]);
    xi.bivec[2] = lr * (p[0] * q[1] - p[1] * q[0]);

    Multivector exp_xi = bch_exp_first_order(xi);
    Multivector evolved{};
    clifford_product(mv, exp_xi, evolved);

    double best_err = 1e300;
    Multivector best = evolved;

    for (int field = 0; field < 3; ++field) {
        int n = (field < 2) ? 3 : 1;
        for (int idx = 0; idx < n; ++idx) {
            Multivector trial = evolved;
            apply_trit_cycle(trial, field, idx);
            double err = std::fabs(hamiltonian(trial) - h0);
            if (err < best_err) {
                best_err = err;
                best = trial;
            }
        }
    }

    word = pack_crank_word(best, depth, static_cast<uint8_t>(word >> 60));
    return 0;
}

struct DecrankTowardCtx {
    const float *W;
};

static double decrank_toward_loss(uint64_t word, void *ctx) {
    auto *c = static_cast<DecrankTowardCtx *>(ctx);
    return decrank_frobenius_loss(word, c->W);
}

int symplectic_turn_toward(uint64_t &word, double lr, const float *target, size_t target_len) {
    if (!target || target_len == 0)
        return symplectic_turn(word, lr);

    float W[64];
    std::memset(W, 0, sizeof(W));
    std::memcpy(W, target, std::min(target_len, size_t(64)) * sizeof(float));
    DecrankTowardCtx ctx{W};
    return symplectic_turn_loss(word, lr, decrank_toward_loss, &ctx);
}

int symplectic_turn_loss(uint64_t &word, double lr, double (*loss_fn)(uint64_t, void *), void *ctx) {
    if (!loss_fn)
        return symplectic_turn(word, lr);

    Multivector mv;
    uint8_t depth;
    unpack_crank_word(word, mv, depth);
    uint8_t flags = static_cast<uint8_t>(word >> 60);
    double loss0 = loss_fn(word, ctx);

    Multivector dXi{};
    for (int field = 0; field < 3; ++field) {
        int n = (field < 2) ? 3 : 1;
        for (int idx = 0; idx < n; ++idx) {
            Multivector trial = mv;
            apply_trit_cycle(trial, field, idx);
            uint64_t trial_word = pack_crank_word(trial, depth, flags);
            double delta = loss_fn(trial_word, ctx) - loss0;
            if (field == 0)
                dXi.vec[idx] -= lr * delta;
            else if (field == 1)
                dXi.bivec[idx] -= lr * delta;
            else
                dXi.trivec -= lr * delta;
        }
    }

    Multivector evolved{};
    clifford_product(mv, bch_exp_first_order(dXi), evolved);
    uint64_t evolved_word = pack_crank_word(evolved, depth, flags);
    double best_loss = loss_fn(evolved_word, ctx);
    Multivector best = evolved;

    for (int field = 0; field < 3; ++field) {
        int n = (field < 2) ? 3 : 1;
        for (int idx = 0; idx < n; ++idx) {
            Multivector trial = evolved;
            apply_trit_cycle(trial, field, idx);
            uint64_t trial_word = pack_crank_word(trial, depth, flags);
            double loss = loss_fn(trial_word, ctx);
            if (loss < best_loss) {
                best_loss = loss;
                best = trial;
            }
        }
    }

    for (int field = 0; field < 3; ++field) {
        int n = (field < 2) ? 3 : 1;
        for (int idx = 0; idx < n; ++idx) {
            Multivector trial = mv;
            apply_trit_cycle(trial, field, idx);
            uint64_t trial_word = pack_crank_word(trial, depth, flags);
            double loss = loss_fn(trial_word, ctx);
            if (loss < best_loss) {
                best_loss = loss;
                best = trial;
            }
        }
    }

    if (best_loss >= loss0)
        return 0;

    word = pack_crank_word(best, depth, flags);
    return 0;
}

struct FinetuneSlotCtx {
    size_t slot_idx;
    const float *target_blocks;
    size_t n_slots;
    crankl_cran_t *cran;
    crankl_loss_fn task_loss;
    void *task_ctx;
    double recon_weight;
    double task_weight;
    uint64_t *slots;
};

static double finetune_slot_loss(uint64_t word, void *ctx) {
    auto *c = static_cast<FinetuneSlotCtx *>(ctx);
    uint64_t saved = c->slots[c->slot_idx];
    c->slots[c->slot_idx] = word;

    double recon = 0.0;
    if (c->target_blocks) {
        const float *W = c->target_blocks + c->slot_idx * 64;
        recon = decrank_frobenius_loss(word, W);
    }

    double task = 0.0;
    if (c->task_loss && c->cran)
        task = c->task_loss(c->cran, c->task_ctx);

    c->slots[c->slot_idx] = saved;
    return c->recon_weight * recon + c->task_weight * task;
}

int symplectic_finetune(uint64_t *slots, size_t n_slots, crankl_cran_t *cran,
                        const float *target_blocks, crankl_loss_fn task_loss, void *task_ctx,
                        int steps, double lr, double recon_weight, double task_weight) {
    if (!slots || n_slots == 0 || steps <= 0)
        return -1;

    for (int step = 0; step < steps; ++step) {
        for (size_t s = 0; s < n_slots; ++s) {
            FinetuneSlotCtx ctx{};
            ctx.slot_idx = s;
            ctx.target_blocks = target_blocks;
            ctx.n_slots = n_slots;
            ctx.cran = cran;
            ctx.task_loss = task_loss;
            ctx.task_ctx = task_ctx;
            ctx.recon_weight = recon_weight;
            ctx.task_weight = task_weight;
            ctx.slots = slots;
            symplectic_turn_loss(slots[s], lr, finetune_slot_loss, &ctx);
        }
    }
    return 0;
}

} // namespace crankl
