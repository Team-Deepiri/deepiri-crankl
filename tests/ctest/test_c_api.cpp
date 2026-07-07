#include "crankle/crankle.h"

#include <cstdio>
#include <cstring>

int main() {
    int fails = 0;

    if (std::strcmp(crankle_version_string(), CRANKLE_VERSION_STRING) != 0) {
        std::fprintf(stderr, "FAIL: version_string mismatch\n");
        ++fails;
    }

    int maj = 0, min = 0, patch = 0;
    crankle_version(&maj, &min, &patch);
    if (maj != CRANKLE_VERSION_MAJOR || min != CRANKLE_VERSION_MINOR ||
        patch != CRANKLE_VERSION_PATCH) {
        std::fprintf(stderr, "FAIL: version components\n");
        ++fails;
    }

    if (std::strcmp(crankle_strerror(CRANKLE_OK), "ok") != 0) {
        std::fprintf(stderr, "FAIL: strerror ok\n");
        ++fails;
    }

    if (crankle_trit_encode(99, nullptr) != CRANKLE_ERR_NULL) {
        std::fprintf(stderr, "FAIL: trit_encode null\n");
        ++fails;
    }

    const char *path = "/tmp/crankle_capi_meta.cran";
    crankle_cran_header_t hdr{};
    hdr.n_slots = 2;
    hdr.gamma = 1.0f;
    uint64_t slots[2] = {0x1, 0x2};
    crankle_cran_metadata_t meta{};
    std::snprintf(meta.model_name, sizeof(meta.model_name), "test-adapter");
    std::snprintf(meta.source_hash, sizeof(meta.source_hash), "deadbeef");

    if (crankle_cran_write_with_metadata(path, &hdr, slots, &meta) != CRANKLE_OK) {
        std::fprintf(stderr, "FAIL: write_with_metadata\n");
        return 1;
    }

    crankle_cran_t cran{};
    if (crankle_cran_read(path, &cran) != CRANKLE_OK) {
        std::fprintf(stderr, "FAIL: read\n");
        return 2;
    }

    crankle_cran_metadata_t out{};
    if (crankle_cran_read_metadata(&cran, &out) != CRANKLE_OK) {
        std::fprintf(stderr, "FAIL: read_metadata\n");
        ++fails;
    } else if (std::strcmp(out.model_name, "test-adapter") != 0 ||
               std::strcmp(out.source_hash, "deadbeef") != 0) {
        std::fprintf(stderr, "FAIL: metadata fields model=%s hash=%s\n", out.model_name,
                     out.source_hash);
        ++fails;
    }

    crankle_cran_close(&cran);

    if (fails == 0)
        std::printf("test_c_api ok\n");
    return fails;
}
