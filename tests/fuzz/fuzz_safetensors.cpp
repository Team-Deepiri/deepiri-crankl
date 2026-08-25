// Coverage-guided fuzz target: .safetensors header/tensor parser.
// Build and run like fuzz_cran (see that file's header).
#include "crankl/crankl.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <filesystem>
#include <string>

namespace {
std::string g_tmp;
} // namespace

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
    (void)argc;
    (void)argv;
    g_tmp = (std::filesystem::temp_directory_path() / "fuzz_st_case.safetensors").string();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 1u << 20)
        return 0;

    FILE *f = std::fopen(g_tmp.c_str(), "wb");
    if (!f)
        return 0;
    std::fwrite(data, 1, size, f);
    std::fclose(f);

    float *tensor = nullptr;
    size_t n = 0;
    if (crankl_safetensors_read_f32(g_tmp.c_str(), "w", &tensor, &n) == CRANKL_OK) {
        // Downstream consumer smoke: metrics over the decoded tensor words.
        crankl_archive_metrics_t metrics{};
        crankl_compute_archive_metrics(reinterpret_cast<const uint64_t *>(tensor), n / 64,
                                       &metrics);
        std::free(tensor);
    }

    size_t count = 0;
    if (crankl_archive_tensor_count(g_tmp.c_str(), &count) == CRANKL_OK && count > 0) {
        std::vector<crankl_archive_tensor_t> list(count);
        crankl_archive_tensor_list(g_tmp.c_str(), list.data(), count);
    }
    return 0;
}
