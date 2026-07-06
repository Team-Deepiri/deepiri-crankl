#include "crankle/crankle.h"

#include <cstdio>
#include <vector>

int main() {
    std::vector<float> data(16);
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<float>(i) * 0.1f;
    std::vector<uint64_t> slots(2);
    if (crankle_pack_f32(data.data(), data.size(), slots.data(), slots.size(), 0.1f, 0.01f) != 0)
        return 1;
    std::vector<float> out(data.size());
    if (crankle_unpack_f32(slots.data(), slots.size(), out.data(), out.size()) != 0)
        return 2;
    std::printf("test_pack ok first=%f\n", out[0]);
    return 0;
}
