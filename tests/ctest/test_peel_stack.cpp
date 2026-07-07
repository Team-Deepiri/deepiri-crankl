#include "crankle/crankle.h"

#include <cstdio>
#include <cstring>
#include <vector>

int main() {
    const size_t n_slots = 3;
    std::vector<uint64_t> slots(n_slots);
    for (size_t i = 0; i < n_slots; ++i) {
        crankle_multivector_t mv{};
        mv.s = static_cast<double>(i) * 0.1;
        mv.v[0] = static_cast<double>(i) * 0.05;
        slots[i] = crankle_crank_from_multivector(&mv, 1);
    }

    std::vector<uint64_t> layer0 = slots;
    std::vector<uint64_t> layer1(n_slots);
    for (size_t i = 0; i < n_slots; ++i) {
        crankle_multivector_t mv{};
        mv.s = static_cast<double>(i) * 0.2;
        layer1[i] = crankle_crank_from_multivector(&mv, 2);
    }

    std::vector<uint64_t> stacks;
    stacks.insert(stacks.end(), layer0.begin(), layer0.end());
    stacks.insert(stacks.end(), layer1.begin(), layer1.end());

    crankle_cran_header_t hdr{};
    hdr.n_slots = n_slots;
    hdr.depth_max = 2;
    hdr.gamma = 1.0f;

    const char *path = "/tmp/crankle_peel_stack_test.cran";
    if (crankle_cran_write(path, &hdr, layer1.data(), stacks.data(), nullptr) != 0)
        return 1;

    crankle_cran_t cran{};
    if (crankle_cran_read(path, &cran) != 0)
        return 2;

    std::vector<uint64_t> peeled(n_slots);
    std::memcpy(peeled.data(), cran.slots, n_slots * sizeof(uint64_t));
    if (crankle_peel_stack(peeled.data(), n_slots, cran.layers, hdr.depth_max, 1) != 0)
        return 3;

    for (size_t i = 0; i < n_slots; ++i) {
        if (peeled[i] != layer0[i]) {
            std::fprintf(stderr, "FAIL: peel_stack slot %zu mismatch\n", i);
            crankle_cran_close(&cran);
            return 4;
        }
    }

    crankle_cran_close(&cran);
    std::printf("test_peel_stack ok\n");
    return 0;
}
