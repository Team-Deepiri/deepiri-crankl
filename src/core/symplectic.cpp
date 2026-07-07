#include "core/internal.hpp"

#include <algorithm>
#include <cmath>

namespace crankle {

static double hamiltonian(const Multivector &mv) {
    double h = mv.s * mv.s;
    for (int i = 0; i < 3; ++i) {
        h += mv.v[i] * mv.v[i];
        h += mv.b[i] * mv.b[i];
    }
    h += mv.p * mv.p;
    return 0.5 * h;
}

static Multivector bch_exp_first_order(const Multivector &xi) {
    Multivector one{};
    one.s = 1.0;
    Multivector xi2{};
    clifford_product(xi, xi, xi2);
    Multivector out{};
    out.s = one.s + xi.s + 0.5 * xi2.s;
    for (int i = 0; i < 3; ++i) {
        out.v[i] = xi.v[i] + 0.5 * xi2.v[i];
        out.b[i] = xi.b[i] + 0.5 * xi2.b[i];
    }
    out.p = xi.p + 0.5 * xi2.p;
    return out;
}

static void apply_trit_cycle(Multivector &mv, int field, int idx) {
    double *target = nullptr;
    if (field == 0)
        target = &mv.v[idx];
    else if (field == 1)
        target = &mv.b[idx];
    else
        target = &mv.p;

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

int symplectic_turn(uint64_t &word, double lr) {
    Multivector mv;
    uint8_t depth;
    unpack_crank_word(word, mv, depth);
    double h0 = hamiltonian(mv);

    // Störmer-Verlet on (q=bivector, p=vector) conjugate pairs.
    double p[3] = {mv.v[0], mv.v[1], mv.v[2]};
    double q[3] = {mv.b[0], mv.b[1], mv.b[2]};

    for (int i = 0; i < 3; ++i)
        p[i] -= 0.5 * lr * q[i];
    for (int i = 0; i < 3; ++i)
        q[i] += lr * p[i];
    for (int i = 0; i < 3; ++i)
        p[i] -= 0.5 * lr * q[i];

    mv.v[0] = p[0];
    mv.v[1] = p[1];
    mv.v[2] = p[2];
    mv.b[0] = q[0];
    mv.b[1] = q[1];
    mv.b[2] = q[2];

    // Lie algebra element (Maurer-Cartan form): bivector rotor from (p, q).
    Multivector xi{};
    xi.b[0] = lr * (p[1] * q[2] - p[2] * q[1]);
    xi.b[1] = lr * (p[2] * q[0] - p[0] * q[2]);
    xi.b[2] = lr * (p[0] * q[1] - p[1] * q[0]);

    Multivector exp_xi = bch_exp_first_order(xi);
    Multivector evolved{};
    clifford_product(mv, exp_xi, evolved);

    // Deterministic trit surgery: pick flip minimizing |ΔH| (symplectic Euler error).
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

static double reconstruction_loss(const Multivector &mv, const float *target, size_t target_len) {
    double vals[8] = {mv.s, mv.v[0], mv.v[1], mv.v[2], mv.b[0], mv.b[1], mv.b[2], mv.p};
    double err = 0.0;
    for (size_t i = 0; i < std::min(target_len, size_t(8)); ++i) {
        double d = vals[i] - static_cast<double>(target[i]);
        err += d * d;
    }
    return err;
}

int symplectic_turn_toward(uint64_t &word, double lr, const float *target, size_t target_len) {
    if (!target || target_len == 0)
        return symplectic_turn(word, lr);

    Multivector mv;
    uint8_t depth;
    unpack_crank_word(word, mv, depth);
    double loss0 = reconstruction_loss(mv, target, target_len);

    double *parts[8] = {&mv.s, &mv.v[0], &mv.v[1], &mv.v[2], &mv.b[0], &mv.b[1], &mv.b[2], &mv.p};
    for (size_t i = 0; i < std::min(target_len, size_t(8)); ++i) {
        double grad = (*parts[i]) - static_cast<double>(target[i]);
        *parts[i] -= lr * grad;
    }

    // Symplectic kick on bivector/vector conjugate pairs after gradient step.
    double p[3] = {mv.v[0], mv.v[1], mv.v[2]};
    double q[3] = {mv.b[0], mv.b[1], mv.b[2]};
    for (int i = 0; i < 3; ++i)
        p[i] -= 0.5 * lr * q[i];
    for (int i = 0; i < 3; ++i)
        q[i] += lr * p[i];
    mv.v[0] = p[0];
    mv.v[1] = p[1];
    mv.v[2] = p[2];
    mv.b[0] = q[0];
    mv.b[1] = q[1];
    mv.b[2] = q[2];

    Multivector xi{};
    xi.b[0] = lr * (p[1] * q[2] - p[2] * q[1]);
    xi.b[1] = lr * (p[2] * q[0] - p[0] * q[2]);
    xi.b[2] = lr * (p[0] * q[1] - p[1] * q[0]);
    Multivector evolved{};
    clifford_product(mv, bch_exp_first_order(xi), evolved);

    double best_loss = reconstruction_loss(evolved, target, target_len);
    Multivector best = evolved;

    for (int field = 0; field < 3; ++field) {
        int n = (field < 2) ? 3 : 1;
        for (int idx = 0; idx < n; ++idx) {
            Multivector trial = evolved;
            apply_trit_cycle(trial, field, idx);
            double loss = reconstruction_loss(trial, target, target_len);
            if (loss < best_loss) {
                best_loss = loss;
                best = trial;
            }
        }
    }

    if (best_loss >= loss0)
        return 0;

    word = pack_crank_word(best, depth, static_cast<uint8_t>(word >> 60));
    return 0;
}

} // namespace crankle
