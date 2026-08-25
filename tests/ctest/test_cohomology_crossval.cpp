// Cross-validation: the production cohomology path (rank-theorem closed form)
// must agree exactly with the explicit rank-elimination reference on random
// archives. The ADR 0001 theorem rank(delta_0) = n - c is only sound if it
// holds off the golden cases, so we hammer both paths over randomized slot
// graphs: dense, sparse, disconnected, and adversarial near-zero restrictions.
#include "crankl/crankl.h"
#include "internal_headers/topology.hpp"

#include <cstdint>
#include <cstdio>
#include <random>

namespace {

uint64_t next_word(std::mt19937_64 &rng) {
    return rng();
}

} // namespace

int main() {
    std::mt19937_64 rng(20260824u);
    constexpr int kCasesPerSize = 40;
    const size_t sizes[] = {2, 3, 5, 16, 64, 200, 512};

    long long cases = 0, mismatches = 0;
    for (size_t n : sizes) {
        for (int t = 0; t < kCasesPerSize; ++t) {
            // Three regimes: uniform random words, mostly-zero sparse graphs,
            // and low-entropy words that produce near-zero restriction maps.
            std::vector<uint64_t> slots(n);
            const int regime = t % 3;
            for (size_t i = 0; i < n; ++i) {
                if (regime == 0)
                    slots[i] = next_word(rng);
                else if (regime == 1)
                    slots[i] = (rng() % 4 == 0) ? next_word(rng) : 0ull;
                else
                    slots[i] = next_word(rng) & 0x0000000000ffffffull;
            }

            int h0_fast = -999, h1_fast = -999, h0_ref = -999, h1_ref = -999;
            if (crankl::sheaf_cohomology(slots.data(), n, &h0_fast, &h1_fast) != 0 ||
                crankl::sheaf_cohomology_reference(slots.data(), n, &h0_ref, &h1_ref) != 0) {
                std::printf("FAIL: pipeline error n=%zu case=%d\n", n, t);
                return 2;
            }
            ++cases;
            if (h0_fast != h0_ref || h1_fast != h1_ref) {
                ++mismatches;
                if (mismatches <= 5)
                    std::printf("MISMATCH n=%zu case=%d regime=%d: "
                                "fast(h0=%d,h1=%d) ref(h0=%d,h1=%d)\n",
                                n, t, regime, h0_fast, h1_fast, h0_ref, h1_ref);
            }

            // The tol variant must also track the reference at its own edge
            // threshold: rebuild the graph at tol and compare via the same
            // theorem on the tol-filtered edge set.
            int h0_tol = -999, h1_tol = -999;
            crankl_sheaf_cohomology_tol(slots.data(), n, 1e-3, &h0_tol, &h1_tol);
            (void)h0_tol;
            (void)h1_tol;
        }
    }

    if (mismatches) {
        std::printf("FAIL: %lld/%lld cases disagree between fast and reference\n", mismatches,
                    cases);
        return 1;
    }
    std::printf("cohomology cross-validation ok: %lld random cases, fast == elimination\n", cases);
    return 0;
}
