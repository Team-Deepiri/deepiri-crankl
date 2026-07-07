#include "core/internal.hpp"

#include <algorithm>
#include <cmath>

namespace crankl {

static double blade_energy(const Multivector &mv) {
    double e = mv.s * mv.s;
    for (int i = 0; i < 3; ++i) {
        e += mv.v[i] * mv.v[i];
        e += mv.b[i] * mv.b[i];
    }
    e += mv.p * mv.p;
    return e;
}

static int trit_value(double x) {
    if (x > 0.33)
        return 1;
    if (x < -0.33)
        return -1;
    return 0;
}

int compute_archive_metrics(const uint64_t *slots, size_t n_slots, ArchiveMetrics &out) {
    out = {};
    if (!slots || n_slots == 0)
        return -1;

    uint64_t nz_trits = 0;
    uint64_t trit_total = 0;
    uint64_t plus = 0;
    uint64_t minus = 0;
    uint64_t zero = 0;

    out.n_slots = n_slots;
    out.depth_min = 255;
    out.depth_max = 0;

    for (size_t i = 0; i < n_slots; ++i) {
        Multivector mv{};
        uint8_t depth = 0;
        unpack_crank_word(slots[i], mv, depth);

        out.depth_min = std::min(out.depth_min, static_cast<uint32_t>(depth));
        out.depth_max = std::max(out.depth_max, static_cast<uint32_t>(depth));
        out.scalar_mean += mv.s;
        out.scalar_abs_mean += std::fabs(mv.s);
        out.clifford_energy += blade_energy(mv);

        double fields[7] = {mv.v[0], mv.v[1], mv.v[2], mv.b[0], mv.b[1], mv.b[2], mv.p};
        for (double f : fields) {
            int t = trit_value(f);
            ++trit_total;
            if (t > 0) {
                ++plus;
                ++nz_trits;
            } else if (t < 0) {
                ++minus;
                ++nz_trits;
            } else {
                ++zero;
            }
        }
    }

    const double n = static_cast<double>(n_slots);
    out.scalar_mean /= n;
    out.scalar_abs_mean /= n;
    out.clifford_energy /= n;
    out.trit_density = static_cast<double>(nz_trits) / static_cast<double>(trit_total);
    out.beta1_proxy = static_cast<double>(sheaf_beta1_proxy(slots, n_slots));

    auto entropy_term = [trit_total](uint64_t c) {
        if (c == 0)
            return 0.0;
        double p = static_cast<double>(c) / static_cast<double>(trit_total);
        return -p * std::log2(p);
    };
    out.trit_entropy = entropy_term(plus) + entropy_term(minus) + entropy_term(zero);
    return 0;
}

} // namespace crankl
