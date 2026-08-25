// Coverage-guided fuzz target: GGUF header/tensor-directory parser.
// Build and run like fuzz_cran (see that file's header).
#include "crankl/crankl.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

namespace {
std::string g_in;
std::string g_out;
} // namespace

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
    (void)argc;
    (void)argv;
    const auto tmp = std::filesystem::temp_directory_path();
    g_in = (tmp / "fuzz_gguf_case.gguf").string();
    g_out = (tmp / "fuzz_gguf_out.crank").string();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 1u << 20)
        return 0;

    FILE *f = std::fopen(g_in.c_str(), "wb");
    if (!f)
        return 0;
    std::fwrite(data, 1, size, f);
    std::fclose(f);

    // The GGUF ingest parses the full header + tensor directory before any
    // extraction; malformed directories must fail cleanly, never crash.
    crankl_pack_gguf_f32(g_in.c_str(), "w", g_out.c_str());
    return 0;
}
