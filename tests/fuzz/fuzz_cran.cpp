// Coverage-guided fuzz target: .crank archive reader.
//
// Build (clang required):
//   cmake -B build-fuzz -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang \
//         -DCRANKL_BUILD_FUZZERS=ON && cmake --build build-fuzz --target fuzz_cran
// Run:
//   ./build-fuzz/tests/fuzz/fuzz_cran -max_total_time=60 corpus/cran
#include "crankl/crankl.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

namespace {
std::string g_tmp;
} // namespace

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
    (void)argc;
    (void)argv;
    g_tmp = (std::filesystem::temp_directory_path() / "fuzz_crank_case.bin").string();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 1u << 20)
        return 0; // cap case size; mmap reader handles big inputs via tests elsewhere

    FILE *f = std::fopen(g_tmp.c_str(), "wb");
    if (!f)
        return 0;
    std::fwrite(data, 1, size, f);
    std::fclose(f);

    crankl_cran_t cran{};
    if (crankl_cran_read(g_tmp.c_str(), &cran) == CRANKL_OK) {
        // Exercise downstream consumers of every parsed field.
        int h0 = 0, h1 = 0;
        crankl_sheaf_cohomology(cran.slots, cran.header.n_slots, &h0, &h1);
        crankl_archive_metrics_t metrics{};
        crankl_cran_compute_metrics(&cran, &metrics);
        crankl_cran_close(&cran);
    }
    return 0;
}
