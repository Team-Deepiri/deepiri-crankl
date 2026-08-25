#include "crankl/crankl.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <thread>
#include <vector>

int main() {
    const size_t n_slots = 3;
    std::vector<uint64_t> slots(n_slots);
    for (size_t i = 0; i < n_slots; ++i) {
        crankl_multivector_t mv{};
        mv.s = static_cast<double>(i) * 0.1;
        mv.v[0] = static_cast<double>(i) * 0.05;
        slots[i] = crankl_crank_from_multivector(&mv, 1);
    }

    std::vector<uint64_t> layer0 = slots;
    std::vector<uint64_t> layer1(n_slots);
    for (size_t i = 0; i < n_slots; ++i) {
        crankl_multivector_t mv{};
        mv.s = static_cast<double>(i) * 0.2;
        layer1[i] = crankl_crank_from_multivector(&mv, 2);
    }

    std::vector<uint64_t> stacks;
    stacks.insert(stacks.end(), layer0.begin(), layer0.end());
    stacks.insert(stacks.end(), layer1.begin(), layer1.end());

    crankl_cran_header_t hdr{};
    hdr.n_slots = n_slots;
    hdr.depth_max = 2;
    hdr.gamma = 1.0f;

    std::string path =
        std::filesystem::temp_directory_path().string() + "/" + "crankl_peel_stack_test.crank";
    if (crankl_cran_write(path.c_str(), &hdr, layer1.data(), stacks.data(), nullptr) != 0)
        return 1;

    crankl_cran_t cran{};
    if (crankl_cran_read(path.c_str(), &cran) != 0)
        return 2;

    std::vector<uint64_t> peeled(n_slots);
    std::memcpy(peeled.data(), cran.slots, n_slots * sizeof(uint64_t));
    if (crankl_peel_stack(peeled.data(), n_slots, cran.layers, hdr.depth_max, 1) != 0)
        return 3;

    for (size_t i = 0; i < n_slots; ++i) {
        if (peeled[i] != layer0[i]) {
            std::fprintf(stderr, "FAIL: peel_stack slot %zu mismatch\n", i);
            crankl_cran_close(&cran);
            return 4;
        }
    }

    crankl_cran_close(&cran);
    std::printf("test_peel_stack ok\n");
    return 0;
}
