#include "crankl/crankl.h"

#include <cstdio>
#include <filesystem>
#include <thread>
#include <cstring>

int main() {
    int fails = 0;

    if (std::strcmp(crankl_version_string(), CRANKL_VERSION_STRING) != 0) {
        std::fprintf(stderr, "FAIL: version_string mismatch\n");
        ++fails;
    }

    int maj = 0, min = 0, patch = 0;
    crankl_version(&maj, &min, &patch);
    if (maj != CRANKL_VERSION_MAJOR || min != CRANKL_VERSION_MINOR ||
        patch != CRANKL_VERSION_PATCH) {
        std::fprintf(stderr, "FAIL: version components\n");
        ++fails;
    }

    if (std::strcmp(crankl_strerror(CRANKL_OK), "ok") != 0) {
        std::fprintf(stderr, "FAIL: strerror ok\n");
        ++fails;
    }

    if (crankl_trit_encode(99, nullptr) != CRANKL_ERR_NULL) {
        std::fprintf(stderr, "FAIL: trit_encode null\n");
        ++fails;
    }

    std::string path = std::filesystem::temp_directory_path().string() + "/" + "crankl_capi_meta.crank";
    crankl_cran_header_t hdr{};
    hdr.n_slots = 2;
    hdr.gamma = 1.0f;
    uint64_t slots[2] = {0x1, 0x2};
    crankl_cran_metadata_t meta{};
    std::snprintf(meta.model_name, sizeof(meta.model_name), "test-adapter");
    std::snprintf(meta.source_hash, sizeof(meta.source_hash), "deadbeef");

    if (crankl_cran_write_with_metadata(path.c_str(), &hdr, slots, &meta) != CRANKL_OK) {
        std::fprintf(stderr, "FAIL: write_with_metadata\n");
        return 1;
    }

    crankl_cran_t cran{};
    if (crankl_cran_read(path.c_str(), &cran) != CRANKL_OK) {
        std::fprintf(stderr, "FAIL: read\n");
        return 2;
    }

    crankl_cran_metadata_t out{};
    if (crankl_cran_read_metadata(&cran, &out) != CRANKL_OK) {
        std::fprintf(stderr, "FAIL: read_metadata\n");
        ++fails;
    } else if (std::strcmp(out.model_name, "test-adapter") != 0 ||
               std::strcmp(out.source_hash, "deadbeef") != 0) {
        std::fprintf(stderr, "FAIL: metadata fields model=%s hash=%s\n", out.model_name,
                     out.source_hash);
        ++fails;
    }

    crankl_cran_close(&cran);

    if (fails == 0)
        std::printf("test_c_api ok\n");
    return fails;
}
