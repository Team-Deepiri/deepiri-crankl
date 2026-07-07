#include "crankle/crankle.h"

#include <cmath>
#include <cstdio>

static double reconstruction_loss(uint64_t w, const float target[8]) {
    crankle_multivector_t cur{};
    uint8_t d = 0;
    crankle_crank_to_multivector(w, &cur, &d);
    double vals[8] = {cur.s, cur.v[0], cur.v[1], cur.v[2], cur.b[0], cur.b[1], cur.b[2], cur.p};
    double err = 0.0;
    for (int i = 0; i < 8; ++i) {
        double diff = vals[i] - target[i];
        err += diff * diff;
    }
    return err;
}

int main() {
    float target[8] = {0.5f, 0.25f, -0.25f, 0.0f, 0.5f, -0.5f, 0.0f, 0.1f};
    crankle_multivector_t mv{};
    mv.s = 0.0;
    uint64_t w = crankle_crank_from_multivector(&mv, 1);

    double loss_before = reconstruction_loss(w, target);
    for (int step = 0; step < 50; ++step)
        crankle_turn_toward(&w, 0.05, target, 8);
    double loss_after = reconstruction_loss(w, target);

    if (loss_after >= loss_before) {
        std::fprintf(stderr, "FAIL: loss did not decrease before=%g after=%g\n", loss_before,
                     loss_after);
        return 1;
    }
    std::printf("test_turn_target ok before=%g after=%g\n", loss_before, loss_after);
    return 0;
}
