#include "crankle/crankle.h"

#include <cmath>
#include <cstdio>
#include <vector>

int main() {
    std::vector<float> data(64);
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<float>(i) * 0.1f - 1.0f;

    const size_t n_slots = 8;
    std::vector<uint64_t> slots(n_slots);
    if (crankle_pack_f32(data.data(), data.size(), slots.data(), n_slots, 0.1f, 0.01f) != 0)
        return 1;

    std::vector<float> out(data.size());
    if (crankle_unpack_f32(slots.data(), n_slots, out.data(), out.size()) != 0)
        return 2;

    double mse = 0.0;
    double max_err = 0.0;
    for (size_t i = 0; i < data.size(); ++i) {
        double e = static_cast<double>(out[i] - data[i]);
        mse += e * e;
        max_err = std::max(max_err, std::fabs(e));
    }
    mse /= static_cast<double>(data.size());

    std::printf("pack_roundtrip mse=%g max_err=%g\n", mse, max_err);
    if (max_err > 5.5) {
        std::fprintf(stderr, "FAIL: max reconstruction error %g > 5.5\n", max_err);
        return 3;
    }
    return 0;
}
