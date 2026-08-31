#include "crankl/crankl.h"

#include <cstdio>
#include <filesystem>
#include <thread>
#include <vector>

int main() {
    std::vector<uint64_t> slots(4, 0);
    crankl_multivector_t mv{0.5, {1, -1, 0}, {0, 1, 0}, 0};
    slots[0] = crankl_crank_from_multivector(&mv, 1);
    crankl_cran_header_t hdr{};
    hdr.n_slots = slots.size();
    hdr.depth_max = 1;
    hdr.gamma = 1.0f;
    std::string path = std::filesystem::temp_directory_path().string() + "/" + "test_crankl.crank";
    if (crankl_cran_write(path.c_str(), &hdr, slots.data(), nullptr, nullptr) != 0)
        return 1;
    crankl_cran_t cran{};
    if (crankl_cran_read(path.c_str(), &cran) != 0)
        return 2;
    if (crankl_cran_verify(&cran) != 0)
        return 3;
    crankl_cran_close(&cran);
    std::printf("test_cran ok\n");
    return 0;
}
