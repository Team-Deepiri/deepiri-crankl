#include "crankle/crankle.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

int main() {
    if (crankle_pack_n_slots(64) != 1 || crankle_pack_n_slots(65) != 2) {
        std::fprintf(stderr, "FAIL: crankle_pack_n_slots\n");
        return 1;
    }

    std::vector<float> data(CRANKLE_BLOCK_FLOATS);
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<float>(std::sin(static_cast<double>(i) * 0.17));

    const size_t n_slots = 1;
    std::vector<uint64_t> slots(n_slots);
    if (crankle_pack_f32(data.data(), data.size(), slots.data(), n_slots, 0.05f, 0.01f) != 0)
        return 1;

    std::vector<float> out(data.size());
    if (crankle_unpack_f32(slots.data(), n_slots, out.data(), out.size()) != 0)
        return 2;

    double frob = 0.0;
    for (size_t i = 0; i < data.size(); ++i) {
        double d = static_cast<double>(out[i] - data[i]);
        frob += d * d;
    }

    std::printf("pack_decrank_roundtrip frob=%g loss=%g\n", frob,
                crankle_decrank_frobenius_loss(slots[0], data.data()));
    if (frob > 200.0) {
        std::fprintf(stderr, "FAIL: decrank Frobenius %g > 200\n", frob);
        return 3;
    }
    return 0;
}
