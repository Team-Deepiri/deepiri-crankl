#include "internal_headers/algebra.hpp"
#include "internal_headers/topology.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace crankl {

static double restriction_map(uint64_t a, uint64_t b) {
    Multivector ma, mb;
    uint8_t da, db;
    unpack_crank_word(a, ma, da);
    unpack_crank_word(b, mb, db);
    (void)da;
    (void)db;

    // Coboundary weight: alignment of bivector stalks + pseudoscalar orientation.
    double align = ma.bivec[0] * mb.bivec[0] + ma.bivec[1] * mb.bivec[1] + ma.bivec[2] * mb.bivec[2];
    double orient = ma.trivec * mb.trivec;
    double vec = ma.vec[0] * mb.vec[0] + ma.vec[1] * mb.vec[1] + ma.vec[2] * mb.vec[2];
    return align + 0.5 * orient + 0.25 * vec;
}

int sheaf_beta1_proxy(const uint64_t *slots, size_t n) {
    if (n < 2)
        return 0;

    // Build restriction graph; β₁ = |E| - |V| + components (for cycle rank).
    int edges = 0;
    std::vector<int> parent(n);
    for (size_t i = 0; i < n; ++i)
        parent[i] = static_cast<int>(i);

    auto find = [&](int x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };

    auto unite = [&](int a, int b) {
        int ra = find(a), rb = find(b);
        if (ra != rb) {
            parent[rb] = ra;
            return true;
        }
        return false;
    };

    for (size_t i = 0; i + 1 < n; ++i) {
        if (std::fabs(restriction_map(slots[i], slots[i + 1])) > 1e-6) {
            ++edges;
            unite(static_cast<int>(i), static_cast<int>(i + 1));
        }
    }

    int components = 0;
    for (size_t i = 0; i < n; ++i) {
        if (find(static_cast<int>(i)) == static_cast<int>(i))
            ++components;
    }

    int beta1 = edges - static_cast<int>(n) + components;
    return std::max(0, beta1);
}

double sheaf_resonance(const uint64_t *slots, size_t n, const uint64_t *other, size_t n_other) {
    size_t m = std::min(n, n_other);
    if (m == 0)
        return 0.0;

    // χ(M₁ ⊗ M₂) proxy: alternating sum of local resonances + coboundary term.
    double chi = 0.0;
    for (size_t i = 0; i < m; ++i) {
        double cl = clifford_resonance(slots[i], other[i]);
        chi += (i % 2 == 0 ? 1.0 : -1.0) * cl;
    }

    double delta = 0.0;
    for (size_t i = 0; i + 1 < m; ++i) {
        double r1 = restriction_map(slots[i], slots[i + 1]);
        double r2 = restriction_map(other[i], other[i + 1]);
        delta += std::fabs(r1 - r2);
    }

    int b1 = sheaf_beta1_proxy(slots, n);
    int b1o = sheaf_beta1_proxy(other, n_other);
    chi += 0.1 * static_cast<double>(b1 - b1o);
    chi -= 0.05 * delta;
    return chi / static_cast<double>(m);
}

} // namespace crankl
