#include "crankl/crankl.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <random>
#include <vector>

static std::vector<float> read_f32(const char *path) {
    FILE *f = std::fopen(path, "rb");
    std::vector<float> out;
    if (!f)
        return out;
    float buf[256];
    size_t r = std::fread(buf, sizeof(float), 256, f);
    std::fclose(f);
    out.assign(buf, buf + r);
    return out;
}

static std::vector<float> rank8_matrix(unsigned seed) {
    std::mt19937 rng(seed);
    auto urand = [&]() {
        return (static_cast<float>(rng()) / 4294967295.0f) * 2.0f - 1.0f;
    };
    std::vector<float> U(64 * 8), V(64 * 8), S(8);
    for (auto &x : U)
        x = urand();
    for (auto &x : V)
        x = urand();
    for (int i = 0; i < 8; ++i)
        S[i] = 10.0f - static_cast<float>(i);
    std::vector<float> out(64 * 64);
    for (int r = 0; r < 64; ++r)
        for (int c = 0; c < 64; ++c) {
            double acc = 0.0;
            for (int k = 0; k < 8; ++k)
                acc += static_cast<double>(U[r * 8 + k]) * S[k] * V[c * 8 + k];
            out[r * 64 + c] = static_cast<float>(acc);
        }
    return out;
}

static double objective(const std::vector<float> &data, const std::vector<uint64_t> &slots,
                        double lambda) {
    double w2 = 0.0, frob = 0.0, obj = 0.0;
    if (crankl_pack_objective(data.data(), data.size(), slots.data(), slots.size(), lambda, &w2,
                              &frob, &obj) != CRANKL_OK)
        return std::nan("");
    return obj;
}

int main() {
    int fails = 0;
    const char *golden = GOLDEN_DIR "/sample.f32";
    const char *small = GOLDEN_DIR "/sample_small.f32";

    if (crankl_pack_default_mode() != CRANKL_PACK_MODE_LEGACY) {
        std::fprintf(stderr, "FAIL: default mode\n");
        ++fails;
    }

    std::vector<std::vector<float>> inputs = {read_f32(golden), read_f32(small),
                                              rank8_matrix(42)};
    if (inputs[0].empty() || inputs[1].empty()) {
        std::fprintf(stderr, "FAIL: could not read golden inputs\n");
        return 1;
    }

    for (const auto &data : inputs) {
        const size_t ns = crankl_pack_n_slots(data.size());

        std::vector<uint64_t> legacy(ns);
        if (crankl_pack_f32(data.data(), data.size(), legacy.data(), ns, 1.0f, 1.0f) !=
            CRANKL_OK) {
            std::fprintf(stderr, "FAIL: pack_f32\n");
            return 1;
        }

        for (unsigned seed : {0u, 7u, 12345u}) {
            std::vector<uint64_t> a(ns);
            std::vector<uint64_t> b(ns);
            if (crankl_pack_f32_anneal(data.data(), data.size(), a.data(), ns, 1.0f, 1.0f,
                                       CRANKL_PACK_MODE_LEGACY, seed) != CRANKL_OK ||
                crankl_pack_f32_anneal(data.data(), data.size(), b.data(), ns, 1.0f, 1.0f,
                                       CRANKL_PACK_MODE_LEGACY, seed) != CRANKL_OK) {
                std::fprintf(stderr, "FAIL: pack_f32_anneal mode 0\n");
                ++fails;
                continue;
            }
            if (a != legacy || b != legacy) {
                std::fprintf(stderr, "FAIL: mode 0 identity to pack_f32\n");
                ++fails;
            }
        }

        for (int mode = CRANKL_PACK_MODE_STAGED; mode <= CRANKL_PACK_MODE_BO; ++mode) {
            std::vector<uint64_t> r1(ns);
            std::vector<uint64_t> r2(ns);
            if (crankl_pack_f32_anneal(data.data(), data.size(), r1.data(), ns, 1.0f, 1.0f, mode,
                                       7u) != CRANKL_OK ||
                crankl_pack_f32_anneal(data.data(), data.size(), r2.data(), ns, 1.0f, 1.0f, mode,
                                       7u) != CRANKL_OK) {
                std::fprintf(stderr, "FAIL: pack_f32_anneal mode %d\n", mode);
                ++fails;
                continue;
            }
            if (r1 != r2) {
                std::fprintf(stderr, "FAIL: mode %d not deterministic\n", mode);
                ++fails;
            }
            double o0 = objective(data, legacy, 1.0);
            double o1 = objective(data, r1, 1.0);
            if (!std::isfinite(o0) || !std::isfinite(o1)) {
                std::fprintf(stderr, "FAIL: mode %d objective not finite\n", mode);
                ++fails;
                continue;
            }
            if (o1 > o0 + 1e-6) {
                std::fprintf(stderr, "FAIL: mode %d objective %g > mode0 %g\n", mode, o1, o0);
                ++fails;
            }
        }
    }

    std::vector<uint64_t> slots(1);
    std::vector<float> data(64);
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<float>(std::sin(static_cast<double>(i) * 0.17));
    if (crankl_pack_f32(data.data(), data.size(), slots.data(), 1, 0.05f, 0.01f) != CRANKL_OK) {
        std::fprintf(stderr, "FAIL: sanity pack\n");
        return 1;
    }
    std::vector<float> recon(64);
    if (crankl_unpack_f32(slots.data(), 1, recon.data(), recon.size()) != CRANKL_OK) {
        std::fprintf(stderr, "FAIL: sanity unpack\n");
        return 1;
    }

    double w2 = 0.0, frob = 0.0, obj = 0.0;
    if (crankl_pack_objective(recon.data(), recon.size(), slots.data(), 1, 1.0, &w2, &frob, &obj) !=
        CRANKL_OK) {
        std::fprintf(stderr, "FAIL: objective identity\n");
        ++fails;
    } else {
        if (std::fabs(w2) > 1e-6 || std::fabs(frob) > 1e-4 || !std::isfinite(obj)) {
            std::fprintf(stderr, "FAIL: identity W2=%g frob=%g obj=%g\n", w2, frob, obj);
            ++fails;
        }
    }

    uint64_t perturbed = slots[0] ^ (1ull << 16);
    std::vector<uint64_t> pslots{perturbed};
    if (crankl_pack_objective(data.data(), data.size(), pslots.data(), 1, 1.0, &w2, &frob, &obj) !=
        CRANKL_OK) {
        std::fprintf(stderr, "FAIL: objective perturbed\n");
        ++fails;
    } else {
        if (!(w2 > 1e-3) || !(frob > 0.0) || !std::isfinite(obj)) {
            std::fprintf(stderr, "FAIL: perturbed W2=%g frob=%g obj=%g\n", w2, frob, obj);
            ++fails;
        }
    }

    if (crankl_pack_objective(nullptr, 0, nullptr, 0, 1.0, &w2, &frob, &obj) != CRANKL_ERR_NULL) {
        std::fprintf(stderr, "FAIL: objective null args\n");
        ++fails;
    }
    if (crankl_pack_f32_anneal(nullptr, 0, nullptr, 0, 1.0f, 1.0f, CRANKL_PACK_MODE_LEGACY, 0) !=
        CRANKL_ERR_NULL) {
        std::fprintf(stderr, "FAIL: anneal null args\n");
        ++fails;
    }
    std::vector<uint64_t> bad(1);
    if (crankl_pack_f32_anneal(data.data(), data.size(), bad.data(), 1, 1.0f, 1.0f, 99, 0) !=
        CRANKL_ERR_INVALID) {
        std::fprintf(stderr, "FAIL: anneal invalid mode\n");
        ++fails;
    }

    {
        std::vector<float> wide(16 * 64);
        for (size_t i = 0; i < wide.size(); ++i)
            wide[i] = static_cast<float>(((i * 2654435761u) >> 13) & 0xFFFF) / 65535.0f;
        std::vector<uint64_t> budget_slots(crankl_pack_n_slots(wide.size()));
        double t0 = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
        crankl_pack_f32_anneal(wide.data(), wide.size(), budget_slots.data(), budget_slots.size(),
                               1.0f, 1.0f, CRANKL_PACK_MODE_STAGED, 5u);
        crankl_pack_f32_anneal(wide.data(), wide.size(), budget_slots.data(), budget_slots.size(),
                               1.0f, 1.0f, CRANKL_PACK_MODE_BO, 5u);
        double t1 = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
        if (t1 - t0 > 30.0) {
            std::fprintf(stderr, "FAIL: budget modes too slow (%g s)\n", t1 - t0);
            ++fails;
        }
    }

    if (fails == 0) {
        std::printf("test_pack_anneal ok\n");
        return 0;
    }
    return 1;
}