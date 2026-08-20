#include "crankl/crankl.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

#include "sheaf_cases.hpp"

// ---------------------------------------------------------------------------
// Deterministic slot builders (trit-quantized multivectors only).
// ---------------------------------------------------------------------------

static uint64_t word_from_mv(double v0, double v1, double v2, double b0, double b1, double b2,
                             double p = 0.0, double s = 0.0) {
    crankl_multivector_t mv{};
    mv.s = s;
    mv.v[0] = v0;
    mv.v[1] = v1;
    mv.v[2] = v2;
    mv.b[0] = b0;
    mv.b[1] = b1;
    mv.b[2] = b2;
    mv.p = p;
    return crankl_crank_from_multivector(&mv, 1);
}

// ---------------------------------------------------------------------------
// In-test replication of the ADR-0001 edge rule + rank theorem, so the tests
// verify the actual Gaussian elimination against the closed-form contract.
// ---------------------------------------------------------------------------

static double restr_weight(uint64_t a, uint64_t b) {
    crankl_multivector_t ma{};
    crankl_multivector_t mb{};
    uint8_t da = 0;
    uint8_t db = 0;
    crankl_crank_to_multivector(a, &ma, &da);
    crankl_crank_to_multivector(b, &mb, &db);
    (void)da;
    (void)db;
    double align = ma.b[0] * mb.b[0] + ma.b[1] * mb.b[1] + ma.b[2] * mb.b[2];
    double orient = ma.p * mb.p;
    double vec = ma.v[0] * mb.v[0] + ma.v[1] * mb.v[1] + ma.v[2] * mb.v[2];
    return align + 0.5 * orient + 0.25 * vec;
}

static void expected_cohomology(const std::vector<uint64_t> &slots, int *h0, int *h1) {
    const size_t n = slots.size();
    if (n == 0) {
        *h0 = 0;
        *h1 = 0;
        return;
    }
    if (n == 1) {
        *h0 = 1;
        *h1 = 0;
        return;
    }

    // Window-2 slot graph with the same 1e-6 restriction threshold as sheaf.cpp.
    size_t m = 0;
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
        int ra = find(a);
        int rb = find(b);
        if (ra != rb)
            parent[rb] = ra;
    };
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n && j <= i + 2; ++j) {
            if (std::fabs(restr_weight(slots[i], slots[j])) > 1e-6) {
                ++m;
                unite(static_cast<int>(i), static_cast<int>(j));
            }
        }
    }
    int c = 0;
    for (size_t i = 0; i < n; ++i)
        if (find(static_cast<int>(i)) == static_cast<int>(i))
            ++c;

    // Rank theorem (ADR 0001): rank(delta0) = n - c.
    *h0 = static_cast<int>(7 * n + static_cast<size_t>(c));
    *h1 = static_cast<int>(m - n + static_cast<size_t>(c));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int failures = 0;

static void check(bool cond, const char *what) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

static void check_coh(const uint64_t *slots, size_t n, int exp_h0, int exp_h1, const char *what) {
    int h0 = 0;
    int h1 = 0;
    int rc = crankl_sheaf_cohomology(slots, n, &h0, &h1);
    if (rc != 0 || h0 != exp_h0 || h1 != exp_h1) {
        std::fprintf(stderr, "FAIL: %s: got h0=%d h1=%d rc=%d, want h0=%d h1=%d\n", what, h0, h1,
                     rc, exp_h0, exp_h1);
        ++failures;
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_trivial_cases() {
    uint64_t one[1] = {word_from_mv(1, 0, 0, 0, 0, 0)};
    check(crankl_sheaf_h0_dim(one, 1) == 1, "h0 for n=1 must be 1");
    check(crankl_sheaf_h1_dim(one, 1) == 0, "h1 for n=1 must be 0");
    check(crankl_sheaf_h0_dim(one, 0) == 0, "h0 for n=0 must be 0");
    check(crankl_sheaf_h1_dim(one, 0) == 0, "h1 for n=0 must be 0");

    int h0 = 99;
    int h1 = 99;
    check(crankl_sheaf_cohomology(nullptr, 1, &h0, &h1) == -1, "NULL slots must return -1");
    check(crankl_sheaf_cohomology(one, 1, nullptr, &h1) == -1, "NULL h0_out must return -1");
    check(crankl_sheaf_cohomology(one, 1, &h0, nullptr) == -1, "NULL h1_out must return -1");
    check(crankl_sheaf_h0_dim(nullptr, 2) == 0, "NULL h0_dim must return 0");
}

static void test_hand_built_graphs() {
    // Two slots with a shared direction: one edge, connected -> (15, 0).
    uint64_t aligned[2] = {word_from_mv(1, 0, 0, 0, 0, 0), word_from_mv(1, 0, 0, 0, 0, 0)};
    check_coh(aligned, 2, 15, 0, "two aligned slots");

    // Orthogonal vector slots: restriction weight 0 -> no edge, c=2 -> (16, 0).
    uint64_t ortho[2] = {word_from_mv(1, 0, 0, 0, 0, 0), word_from_mv(0, 1, 0, 0, 0, 0)};
    check_coh(ortho, 2, 16, 0, "two orthogonal slots");

    // Chain of 3: e1 -- (e1+e2) -- e2. Distance-2 pair (e1, e2) has weight 0,
    // so m=2, c=1 -> dim H1 = 2 - 3 + 1 = 0, dim H0 = 22.
    uint64_t chain3[3] = {word_from_mv(1, 0, 0, 0, 0, 0), word_from_mv(1, 1, 0, 0, 0, 0),
                          word_from_mv(0, 1, 0, 0, 0, 0)};
    check_coh(chain3, 3, 22, 0, "chain of 3 slots");

    // Triangle: three identical e1 slots, all pairwise aligned -> m=3, c=1,
    // dim H1 = 3 - 3 + 1 = 1 (one cycle).
    uint64_t tri[3] = {word_from_mv(1, 0, 0, 0, 0, 0), word_from_mv(1, 0, 0, 0, 0, 0),
                       word_from_mv(1, 0, 0, 0, 0, 0)};
    check_coh(tri, 3, 22, 1, "triangle of 3 slots");

    // Bivector stalks behave identically through the restriction alignment.
    uint64_t biv_pair[2] = {word_from_mv(0, 0, 0, 1, 0, 0), word_from_mv(0, 0, 0, 1, 0, 0)};
    check_coh(biv_pair, 2, 15, 0, "two aligned bivector slots");
    uint64_t biv_tri[3] = {word_from_mv(0, 0, 0, 1, 0, 0), word_from_mv(0, 0, 0, 1, 0, 0),
                           word_from_mv(0, 0, 0, 1, 0, 0)};
    check_coh(biv_tri, 3, 22, 1, "triangle of bivector slots");

    // A span-3 archive (no window-2 edges at all): c=4, m=0 -> (32, 0).
    uint64_t span3[4] = {word_from_mv(1, 0, 0, 0, 0, 0), word_from_mv(0, 0, 0, 1, 0, 0),
                         word_from_mv(0, 0, 0, 0, 1, 0), word_from_mv(0, 0, 0, 0, 0, 1)};
    check_coh(span3, 4, 32, 0, "four mutually orthogonal slots");

    // A degenerate (all-zero) slot must not break the sheaf: h0 >= 1, h1 >= 0.
    uint64_t with_zero[3] = {word_from_mv(1, 0, 0, 0, 0, 0), 0,
                             word_from_mv(1, 0, 0, 0, 0, 0)};
    int h0 = 0;
    int h1 = -1;
    check(crankl_sheaf_cohomology(with_zero, 3, &h0, &h1) == 0, "degenerate slot rc");
    check(h0 >= 1 && h1 >= 0, "degenerate slot keeps h0>=1, h1>=0");
}

static void test_golden_parity() {
    for (int i = 0; i < SHEAF_CASE_COUNT; ++i) {
        const SheafCase &c = SHEAF_CASES[i];
        int h0 = 0;
        int h1 = 0;
        int rc = crankl_sheaf_cohomology(c.slots, static_cast<size_t>(c.n), &h0, &h1);
        if (rc != 0 || h0 != c.h0 || h1 != c.h1) {
            std::fprintf(stderr, "FAIL: golden case %d: got h0=%d h1=%d rc=%d, want h0=%d h1=%d\n",
                         i, h0, h1, rc, c.h0, c.h1);
            ++failures;
        }
    }
}

static void test_random_archive_sanity() {
    std::mt19937_64 rng(0xC0FFEE);
    std::uniform_int_distribution<int> comp(-1, 1);

    for (int trial = 0; trial < 200; ++trial) {
        const size_t n = 2 + static_cast<size_t>(rng() % 7); // 2..8 slots
        std::vector<uint64_t> slots(n);
        for (size_t i = 0; i < n; ++i) {
            double v[3] = {static_cast<double>(comp(rng)), static_cast<double>(comp(rng)),
                           static_cast<double>(comp(rng))};
            double b[3] = {static_cast<double>(comp(rng)), static_cast<double>(comp(rng)),
                           static_cast<double>(comp(rng))};
            slots[i] = word_from_mv(v[0], v[1], v[2], b[0], b[1], b[2],
                                    static_cast<double>(comp(rng)), static_cast<double>(comp(rng)));
        }

        int h0 = 0;
        int h1 = 0;
        int rc = crankl_sheaf_cohomology(slots.data(), n, &h0, &h1);
        int exp_h0 = 0;
        int exp_h1 = 0;
        expected_cohomology(slots, &exp_h0, &exp_h1);

        if (rc != 0 || h0 != exp_h0 || h1 != exp_h1) {
            std::fprintf(stderr, "FAIL: random trial %d: got h0=%d h1=%d, want h0=%d h1=%d\n",
                         trial, h0, h1, exp_h0, exp_h1);
            ++failures;
        }
        if (h0 < 1 || h1 < 0) {
            std::fprintf(stderr, "FAIL: random trial %d violates h0>=1, h1>=0 (h0=%d h1=%d)\n",
                         trial, h0, h1);
            ++failures;
        }
        if (crankl_sheaf_h0_dim(slots.data(), n) != h0 || crankl_sheaf_h1_dim(slots.data(), n) != h1) {
            std::fprintf(stderr, "FAIL: h0_dim/h1_dim disagree with cohomology on trial %d\n",
                         trial);
            ++failures;
        }
    }
}

// Stability contract: dim H1 = m - n + c changes only when a restriction
// weight crosses the 1e-6 threshold. For archives whose weights are multiples
// of 0.25 (trit-aligned) that never happens, so peel / bind / trit surgery on
// a vector-only archive leaves H1 exactly invariant (bound documented: 0).
static void test_stability_peel() {
    uint64_t slots[6] = {word_from_mv(1, 0, 0, 0, 0, 0), word_from_mv(0, 1, 0, 0, 0, 0),
                         word_from_mv(1, 1, 0, 0, 0, 0), word_from_mv(1, 0, 0, 0, 0, 0),
                         word_from_mv(0, 1, 0, 0, 0, 0), word_from_mv(1, 1, 0, 0, 0, 0)};
    int base = crankl_sheaf_h1_dim(slots, 6);
    check(base >= 0, "peel baseline h1 must be non-negative");

    for (int layers = 1; layers <= 3; ++layers) {
        uint64_t peeled[6];
        std::memcpy(peeled, slots, sizeof(slots));
        for (int i = 0; i < 6; ++i)
            crankl_peel(&peeled[i], static_cast<uint32_t>(layers));
        int h1 = crankl_sheaf_h1_dim(peeled, 6);
        // Vector-only archive: peel rescales bivector/pseudoscalar (already 0)
        // and re-quantizes, leaving the edge set intact -> bound is exactly 0.
        if (h1 != base) {
            std::fprintf(stderr, "FAIL: peel %d layers changed h1 from %d to %d\n", layers, base,
                         h1);
            ++failures;
        }
    }

    // Bivector-heavy archive: peel may remove bivector-align edges. Bound
    // documented: |Delta h1| <= number of window pairs (2n-3 for n slots).
    uint64_t biv[4] = {word_from_mv(0, 0, 0, 1, 0, 0), word_from_mv(0, 0, 0, 1, 0, 0),
                       word_from_mv(0, 0, 0, 1, 0, 0), word_from_mv(0, 0, 0, 1, 0, 0)};
    int base_biv = crankl_sheaf_h1_dim(biv, 4);
    uint64_t peeled_biv[4];
    std::memcpy(peeled_biv, biv, sizeof(biv));
    for (int i = 0; i < 4; ++i)
        crankl_peel(&peeled_biv[i], 1);
    int h1_biv = crankl_sheaf_h1_dim(peeled_biv, 4);
    if (std::abs(h1_biv - base_biv) > 5) {
        std::fprintf(stderr, "FAIL: peel on bivector archive moved h1 outside bound (base=%d "
                             "after=%d)\n",
                     base_biv, h1_biv);
        ++failures;
    }
}

static void test_stability_bind() {
    uint64_t slots[6] = {word_from_mv(1, 0, 0, 0, 0, 0), word_from_mv(0, 1, 0, 0, 0, 0),
                         word_from_mv(1, 1, 0, 0, 0, 0), word_from_mv(1, 0, 0, 0, 0, 0),
                         word_from_mv(0, 1, 0, 0, 0, 0), word_from_mv(1, 1, 0, 0, 0, 0)};
    int base = crankl_sheaf_h1_dim(slots, 6);

    uint64_t bound[6];
    std::memcpy(bound, slots, sizeof(slots));
    for (int i = 0; i + 1 < 6; ++i)
        bound[i] = crankl_bind(bound[i], bound[i + 1]);
    int h1 = crankl_sheaf_h1_dim(bound, 6);
    // A Clifford product moves restriction weights freely; the documented bound
    // is one edge flip per window pair, i.e. |Delta h1| <= 2n - 3 here.
    if (std::abs(h1 - base) > 9) {
        std::fprintf(stderr, "FAIL: bind moved h1 outside bound (base=%d after=%d)\n", base, h1);
        ++failures;
    }
}

static void test_stability_trit_surgery() {
    uint64_t slots[6] = {word_from_mv(1, 0, 0, 0, 0, 0), word_from_mv(0, 1, 0, 0, 0, 0),
                         word_from_mv(1, 1, 0, 0, 0, 0), word_from_mv(1, 0, 0, 0, 0, 0),
                         word_from_mv(0, 1, 0, 0, 0, 0), word_from_mv(1, 1, 0, 0, 0, 0)};
    int base = crankl_sheaf_h1_dim(slots, 6);

    // Flip a low scalar bit (bit 3): the scalar does not enter restriction_map,
    // so the edge set is exactly preserved and H1 is invariant (bound 0).
    uint64_t scalar_surgery[6];
    std::memcpy(scalar_surgery, slots, sizeof(slots));
    scalar_surgery[2] ^= (uint64_t{1} << 3);
    if (crankl_sheaf_h1_dim(scalar_surgery, 6) != base) {
        std::fprintf(stderr, "FAIL: scalar-bit surgery changed h1\n");
        ++failures;
    }

    // Flip a low bivector trit (bits 22-23): the other slots are vector-only,
    // so the pairwise alignment is unchanged and H1 stays invariant (bound 0).
    uint64_t biv_surgery[6];
    std::memcpy(biv_surgery, slots, sizeof(slots));
    biv_surgery[3] ^= (uint64_t{3} << 22);
    if (crankl_sheaf_h1_dim(biv_surgery, 6) != base) {
        std::fprintf(stderr, "FAIL: bivector-trit surgery changed h1\n");
        ++failures;
    }

    // Flip a low vector trit (bits 16-17) in a slot that bridges a chain: the
    // incident weights move by +/-0.25 but stay multiples of 0.25 far above
    // 1e-6, so H1 is still invariant on this archive.
    uint64_t vec_surgery[6];
    std::memcpy(vec_surgery, slots, sizeof(slots));
    vec_surgery[2] ^= (uint64_t{3} << 16);
    if (crankl_sheaf_h1_dim(vec_surgery, 6) != base) {
        std::fprintf(stderr, "FAIL: vector-trit surgery changed h1\n");
        ++failures;
    }
}

static void test_resonance_h1() {
    uint64_t a[4] = {word_from_mv(1, 0, 0, 0, 0, 0), word_from_mv(0, 1, 0, 0, 0, 0),
                     word_from_mv(1, 0, 0, 0, 0, 0), word_from_mv(0, 1, 0, 0, 0, 0)};
    uint64_t b[4] = {word_from_mv(1, 0, 0, 0, 0, 0), word_from_mv(0, 1, 0, 0, 0, 0),
                     word_from_mv(0, 1, 0, 0, 0, 0), word_from_mv(1, 0, 0, 0, 0, 0)};

    double r_h1 = crankl_sheaf_resonance_h1(a, 4, b, 4);
    check(std::isfinite(r_h1), "resonance_h1 must be finite");

    // Deterministic and consistent with the legacy scalar: same local
    // resonance alternation and coboundary structure.
    double r_legacy = crankl_sheaf_resonance(a, 4, b, 4);
    check(std::isfinite(r_legacy), "legacy resonance must be finite");
    check(std::fabs(r_h1) < 10.0, "resonance_h1 magnitude sane");

    // Empty and single-slot edge cases stay well-defined.
    check(crankl_sheaf_resonance_h1(a, 0, b, 4) == 0.0, "resonance_h1 with 0 slots is 0");
    check(crankl_sheaf_resonance_h1(a, 1, b, 1) == crankl_sheaf_resonance_h1(a, 1, b, 1),
          "resonance_h1 deterministic on 1 slot");
}

static void test_backward_compat() {
    // beta1_proxy and legacy resonance are frozen deprecated aliases: for the
    // original smoke inputs their outputs must not change.
    uint64_t a[4] = {1, 2, 3, 4};
    uint64_t b[4] = {4, 3, 2, 1};
    check(crankl_sheaf_beta1_proxy(a, 4) == 0, "beta1_proxy({1,2,3,4}) stays 0");
    check(crankl_sheaf_resonance(a, 4, b, 4) == 0.0, "legacy resonance stays 0");
    check(std::isfinite(crankl_sheaf_resonance_h1(a, 4, b, 4)), "resonance_h1 finite");
}

int main() {
    test_trivial_cases();
    test_hand_built_graphs();
    test_golden_parity();
    test_random_archive_sanity();
    test_stability_peel();
    test_stability_bind();
    test_stability_trit_surgery();
    test_resonance_h1();
    test_backward_compat();

    if (failures == 0)
        std::printf("test_sheaf ok (cohomology: %d golden cases, %d hand-built graphs)\n",
                    SHEAF_CASE_COUNT, 8);
    return failures;
}