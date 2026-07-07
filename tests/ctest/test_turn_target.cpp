#include "crankle/crankle.h"

#include <cmath>
#include <cstdio>
#include <vector>

int main() {
    float target[CRANKLE_BLOCK_FLOATS];
    for (int i = 0; i < CRANKLE_BLOCK_FLOATS; ++i)
        target[i] = 0.3f * std::cos(static_cast<float>(i + CRANKLE_BLOCK_FLOATS) * 0.11f);

    std::vector<uint64_t> slots(1);
    if (crankle_pack_f32(target, CRANKLE_BLOCK_FLOATS, slots.data(), 1, 0.03f, 0.005f) != 0)
        return 1;

    uint64_t w = slots[0];
    double loss_before = crankle_decrank_frobenius_loss(w, target);
    for (int step = 0; step < 80; ++step)
        crankle_turn_toward(&w, 0.1, target, CRANKLE_BLOCK_FLOATS);
    double loss_after = crankle_decrank_frobenius_loss(w, target);

    if (!(loss_after < loss_before * 0.99)) {
        std::fprintf(stderr, "FAIL: decrank loss did not decrease before=%g after=%g\n", loss_before,
                     loss_after);
        return 2;
    }
    std::printf("test_turn_target ok before=%g after=%g\n", loss_before, loss_after);
    return 0;
}
