#include "internal_headers/algebra.hpp"
#include "internal_headers/topology.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace crankl {

// Restriction-sheaf constants (ADR 0001): the slot graph connects index
// neighbors at distance <= kSheafWindow, an edge survives when its restriction
// weight exceeds kSheafTol, and Gaussian elimination accepts pivots above
// kPivotTol (entries are O(1), so this is ~7 orders above machine epsilon).
constexpr double kSheafTol = 1e-6;
constexpr double kPivotTol = 1e-9;
constexpr int kSheafWindow = 2;

static double restriction_map(uint64_t a, uint64_t b) {
    Multivector ma, mb;
    uint8_t da, db;
    unpack_crank_word(a, ma, da);
    unpack_crank_word(b, mb, db);
    (void)da;
    (void)db;

    // Coboundary weight: alignment of bivector stalks + pseudoscalar orientation.
    double align =
        ma.bivec[0] * mb.bivec[0] + ma.bivec[1] * mb.bivec[1] + ma.bivec[2] * mb.bivec[2];
    double orient = ma.trivec * mb.trivec;
    double vec = ma.vec[0] * mb.vec[0] + ma.vec[1] * mb.vec[1] + ma.vec[2] * mb.vec[2];
    return align + 0.5 * orient + 0.25 * vec;
}

// Unit multivector direction of a slot: the normalized 8-component vector of
// the unpacked multivector, used as the edge-restriction projection rho_{i<=e}.
// A (near-)zero slot gets the canonical fallback direction e1 so the constant
// global section f_i = c * u_i stays well-defined on every slot.
static void slot_direction(uint64_t word, double dir[8]) {
    Multivector mv;
    uint8_t depth;
    unpack_crank_word(word, mv, depth);
    (void)depth;

    double comp[8] = {mv.scalar,  mv.vec[0],  mv.vec[1],  mv.vec[2],
                      mv.bivec[0], mv.bivec[1], mv.bivec[2], mv.trivec};
    double norm = 0.0;
    for (int k = 0; k < 8; ++k)
        norm += comp[k] * comp[k];
    norm = std::sqrt(norm);
    if (norm <= 1e-9) {
        dir[0] = 1.0;
        for (int k = 1; k < 8; ++k)
            dir[k] = 0.0;
        return;
    }
    for (int k = 0; k < 8; ++k)
        dir[k] = comp[k] / norm;
}

struct SheafComplex {
    size_t n_slots = 0;
    size_t m_edges = 0;
    std::vector<size_t> edge_u;
    std::vector<size_t> edge_v;
    std::vector<std::array<double, 8>> dir;
};

// Build the restriction slot graph: vertices are slots, edges are index
// neighbors at distance <= kSheafWindow with |restriction_map| > kSheafTol.
static void build_sheaf(const uint64_t *slots, size_t n, SheafComplex &out) {
    out.n_slots = n;
    out.edge_u.clear();
    out.edge_v.clear();
    out.dir.resize(n);
    for (size_t i = 0; i < n; ++i)
        slot_direction(slots[i], out.dir[i].data());

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n && j <= i + static_cast<size_t>(kSheafWindow); ++j) {
            if (std::fabs(restriction_map(slots[i], slots[j])) > kSheafTol) {
                out.edge_u.push_back(i);
                out.edge_v.push_back(j);
            }
        }
    }
    out.m_edges = out.edge_u.size();
}

using SparseRow = std::vector<std::pair<size_t, double>>;

static double value_at(const SparseRow &row, size_t col) {
    for (const auto &entry : row) {
        if (entry.first == col)
            return entry.second;
        if (entry.first > col)
            break;
    }
    return 0.0;
}

// Row of delta_0 for edge e = (u, v): (delta_0 f)_e = <u_v, f_v> - <u_u, f_u>.
static SparseRow build_delta_row(size_t u, size_t v, const double uu[8], const double uv[8]) {
    SparseRow row;
    row.reserve(16);
    for (int k = 0; k < 8; ++k) {
        if (std::fabs(uu[k]) > kPivotTol)
            row.push_back({8 * u + static_cast<size_t>(k), -uu[k]});
        if (std::fabs(uv[k]) > kPivotTol)
            row.push_back({8 * v + static_cast<size_t>(k), uv[k]});
    }
    std::sort(row.begin(), row.end());
    return row;
}

static SparseRow row_sub(const SparseRow &a, const SparseRow &b, double factor) {
    SparseRow out;
    out.reserve(a.size() + b.size());
    size_t ia = 0;
    size_t ib = 0;
    while (ia < a.size() || ib < b.size()) {
        size_t ca = ia < a.size() ? a[ia].first : static_cast<size_t>(-1);
        size_t cb = ib < b.size() ? b[ib].first : static_cast<size_t>(-1);
        double val;
        if (ca == cb && ca != static_cast<size_t>(-1)) {
            val = a[ia].second - factor * b[ib].second;
            ++ia;
            ++ib;
        } else if (ca < cb) {
            val = a[ia].second;
            ++ia;
        } else {
            val = -factor * b[ib].second;
            ++ib;
        }
        if (std::fabs(val) > kPivotTol)
            out.push_back({std::min(ca, cb), val});
    }
    return out;
}

// Integer rank of the sparse m x (8n) coboundary matrix via Gaussian
// elimination with partial pivoting. Each row carries at most 2 x 8 nonzero
// entries, so elimination touches O(m) entries per pivot column and the whole
// pass is O(n^2) worst case (ADR 0001).
static int gaussian_rank(const std::vector<SparseRow> &rows, size_t n_cols) {
    std::vector<SparseRow> active = rows;
    int rank = 0;
    for (size_t col = 0; col < n_cols; ++col) {
        SparseRow *pivot = nullptr;
        for (auto &row : active) {
            if (std::fabs(value_at(row, col)) > kPivotTol) {
                pivot = &row;
                break;
            }
        }
        if (!pivot)
            continue;

        double pivot_val = value_at(*pivot, col);
        for (auto &entry : *pivot)
            entry.second /= pivot_val;

        std::vector<SparseRow> next;
        next.reserve(active.size());
        for (auto &row : active) {
            if (&row == pivot)
                continue;
            double factor = value_at(row, col);
            if (std::fabs(factor) <= kPivotTol) {
                next.push_back(std::move(row));
                continue;
            }
            SparseRow reduced = row_sub(row, *pivot, factor);
            if (!reduced.empty())
                next.push_back(std::move(reduced));
        }
        active = std::move(next);
        ++rank;
    }
    return rank;
}

int sheaf_cohomology(const uint64_t *slots, size_t n, int *h0_out, int *h1_out) {
    if (!slots || !h0_out || !h1_out)
        return -1;
    *h0_out = 0;
    *h1_out = 0;
    if (n == 0)
        return 0;
    if (n == 1) {
        *h0_out = 1;
        *h1_out = 0;
        return 0;
    }

    SheafComplex sc;
    build_sheaf(slots, n, sc);
    std::vector<SparseRow> rows;
    rows.reserve(sc.m_edges);
    for (size_t e = 0; e < sc.m_edges; ++e) {
        const double *du = sc.dir[sc.edge_u[e]].data();
        const double *dv = sc.dir[sc.edge_v[e]].data();
        rows.push_back(build_delta_row(sc.edge_u[e], sc.edge_v[e], du, dv));
    }

    int rank = gaussian_rank(rows, 8 * n);
    *h0_out = static_cast<int>(8 * n - static_cast<size_t>(rank));
    *h1_out = static_cast<int>(sc.m_edges - static_cast<size_t>(rank));
    return 0;
}

int sheaf_h0_dim(const uint64_t *slots, size_t n) {
    int h0 = 0;
    int h1 = 0;
    if (sheaf_cohomology(slots, n, &h0, &h1) != 0)
        return 0;
    return h0;
}

int sheaf_h1_dim(const uint64_t *slots, size_t n) {
    int h0 = 0;
    int h1 = 0;
    if (sheaf_cohomology(slots, n, &h0, &h1) != 0)
        return 0;
    return h1;
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

// Resonance over the H1 cohomology instead of the beta1 proxy: the local
// alternating sum is unchanged, the coboundary term compares restriction maps
// over the full window-2 slot graph, and the topology term uses dim H1.
double sheaf_resonance_h1(const uint64_t *slots, size_t n, const uint64_t *other,
                          size_t n_other) {
    size_t m = std::min(n, n_other);
    if (m == 0)
        return 0.0;

    double chi = 0.0;
    for (size_t i = 0; i < m; ++i) {
        double cl = clifford_resonance(slots[i], other[i]);
        chi += (i % 2 == 0 ? 1.0 : -1.0) * cl;
    }

    double delta = 0.0;
    for (size_t i = 0; i + 1 < m; ++i) {
        for (size_t j = i + 1; j < m && j <= i + static_cast<size_t>(kSheafWindow); ++j) {
            double r1 = restriction_map(slots[i], slots[j]);
            double r2 = restriction_map(other[i], other[j]);
            delta += std::fabs(r1 - r2);
        }
    }

    int h1a = sheaf_h1_dim(slots, n);
    int h1b = sheaf_h1_dim(other, n_other);
    chi += 0.1 * static_cast<double>(h1a - h1b);
    chi -= 0.05 * delta;
    return chi / static_cast<double>(m);
}

} // namespace crankl
