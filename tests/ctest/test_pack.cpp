#include "crankl/crankl.h"

#include <cstdio>
#include <vector>

int main() {
    if (crankl_pack_default_mode() != CRANKL_PACK_MODE_LEGACY) {
        std::fprintf(stderr, "FAIL: pack default mode\n");
        return 1;
    }
    std::vector<float> data(16);
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<float>(i) * 0.1f;
    std::vector<uint64_t> slots(2);
    if (crankl_pack_f32(data.data(), data.size(), slots.data(), slots.size(), 0.1f, 0.01f) != 0)
        return 1;
    std::vector<uint64_t> anneal_slots(2);
    if (crankl_pack_f32_anneal(data.data(), data.size(), anneal_slots.data(), anneal_slots.size(),
                               0.1f, 0.01f, CRANKL_PACK_MODE_LEGACY, 42u) != 0)
        return 1;
    if (anneal_slots != slots) {
        std::fprintf(stderr, "FAIL: mode 0 anneal != pack_f32\n");
        return 1;
    }
    std::vector<float> out(data.size());
    if (crankl_unpack_f32(slots.data(), slots.size(), out.data(), out.size()) != 0)
        return 2;
    std::printf("test_pack ok first=%f\n", out[0]);
    return 0;
}
