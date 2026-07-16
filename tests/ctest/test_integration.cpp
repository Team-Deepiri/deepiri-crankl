#include "crankl/crankl.h"

#include <cstdio>
#include <cstring>
#include <vector>

int main() {
    std::vector<float> data(16);
    for (int i = 0; i < 16; ++i)
        data[i] = static_cast<float>(i) * 0.1f;

    const char *path = "/tmp/crankl_integration.crank";
    const char *tuned = "/tmp/crankl_integration_tuned.crank";

    std::vector<uint64_t> slots(crankl_pack_n_slots(data.size()));
    if (crankl_pack_f32(data.data(), data.size(), slots.data(), slots.size(), 0.1f, 0.01f) != 0)
        return 1;

    crankl_cran_header_t hdr{};
    hdr.n_slots = slots.size();
    hdr.depth_max = 4;
    hdr.gamma = 0.7f;
    if (crankl_cran_write(path, &hdr, slots.data(), nullptr, nullptr) != 0)
        return 2;

    crankl_cran_t cran{};
    if (crankl_cran_read(path, &cran) != 0)
        return 4;
    if (crankl_cran_verify(&cran) != 0)
        return 5;

    std::vector<uint64_t> tuned_slots(slots);
    for (int step = 0; step < 5; ++step) {
        for (auto &w : tuned_slots)
            crankl_turn(&w, 0.02);
    }
    if (crankl_cran_write(tuned, &hdr, tuned_slots.data(), nullptr, nullptr) != 0)
        return 6;

    crankl_cran_t cran2{};
    std::vector<uint64_t> loaded(cran.header.n_slots);
    if (crankl_cran_read(tuned, &cran2) != 0)
        return 7;
    std::memcpy(loaded.data(), cran2.slots, loaded.size() * sizeof(uint64_t));

    size_t changed = crankl_crank_diff_count(slots.data(), loaded.data(), slots.size());
    double ham = crankl_crank_diff_hamming(slots.data(), loaded.data(), slots.size());
    if (changed == 0) {
        std::fprintf(stderr, "FAIL: turn should change slots\n");
        crankl_cran_close(&cran);
        crankl_cran_close(&cran2);
        return 8;
    }

    float x[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    float y[8] = {};
    crankl_holonomy(&cran2, x, 8, y);

    crankl_cran_close(&cran);
    crankl_cran_close(&cran2);
    std::printf("test_integration ok changed=%zu hamming=%g y0=%g\n", changed, ham, y[0]);
    return 0;
}
