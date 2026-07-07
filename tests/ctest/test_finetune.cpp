#include "crankle/crankle.h"

#include <cmath>
#include <cstdio>
#include <vector>

int main() {
    const size_t n_slots = 2;
    std::vector<float> target(n_slots * CRANKLE_BLOCK_FLOATS);
    for (size_t i = 0; i < target.size(); ++i)
        target[i] = 0.3f * std::cos(static_cast<float>(i) * 0.11f);

    std::vector<uint64_t> slots(n_slots);
    if (crankle_pack_f32(target.data(), target.size(), slots.data(), n_slots, 0.03f, 0.005f) != 0)
        return 1;

    crankle_cran_header_t hdr{};
    hdr.n_slots = n_slots;
    hdr.depth_max = 1;
    hdr.gamma = 1.0f;

    crankle_cran_t view{};
    view.header = hdr;
    view.slots = slots.data();

    double recon_before = 0.0;
    for (size_t s = 0; s < n_slots; ++s)
        recon_before += crankle_decrank_frobenius_loss(slots[s], target.data() + s * CRANKLE_BLOCK_FLOATS);

    if (crankle_finetune(slots.data(), n_slots, &view, target.data(), nullptr, nullptr, 80, 0.1, 1.0,
                         0.0) != 0)
        return 2;

    double recon_after = 0.0;
    for (size_t s = 0; s < n_slots; ++s)
        recon_after += crankle_decrank_frobenius_loss(slots[s], target.data() + s * CRANKLE_BLOCK_FLOATS);

    if (!(recon_after < recon_before * 0.99)) {
        std::fprintf(stderr, "FAIL: finetune recon before=%g after=%g\n", recon_before, recon_after);
        return 3;
    }

    std::printf("test_finetune ok before=%g after=%g\n", recon_before, recon_after);
    return 0;
}
