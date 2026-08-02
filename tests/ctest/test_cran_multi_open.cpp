#include "crankl/crankl.h"

#include <cstdio>
#include <cstring>
#include <vector>

// Two archives held open at once must keep independent mappings. Before per-handle
// ownership, reading either handle after the second open faulted, and closing one handle
// tore down the other.

static const uint64_t FILL_A = 0xAAAAAAAAAAAAAAAAULL;
static const uint64_t FILL_B = 0xBBBBBBBBBBBBBBBBULL;

static int write_filled(const char *path, uint64_t fill, uint64_t n_slots, const char *model) {
    std::vector<uint64_t> slots(static_cast<size_t>(n_slots), fill);
    crankl_cran_header_t hdr{};
    hdr.n_slots = n_slots;
    hdr.depth_max = 1;
    hdr.gamma = 1.0f;
    if (model) {
        crankl_cran_metadata_t meta{};
        std::snprintf(meta.model_name, sizeof(meta.model_name), "%s", model);
        return crankl_cran_write_with_metadata(path, &hdr, slots.data(), &meta);
    }
    return crankl_cran_write(path, &hdr, slots.data(), nullptr, nullptr);
}

// Slots start at the 124-byte packed header, so they are not 8-byte aligned; copy the
// word out rather than dereferencing, as test_integration and test_peel_stack do.
static uint64_t first_slot(const crankl_cran_t *cran) {
    uint64_t w = 0;
    std::memcpy(&w, cran->slots, sizeof(w));
    return w;
}

int main() {
    const char *path_a = "/tmp/crankl_multi_a.crank";
    const char *path_b = "/tmp/crankl_multi_b.crank";

    if (write_filled(path_a, FILL_A, 4, nullptr) != CRANKL_OK)
        return 1;
    if (write_filled(path_b, FILL_B, 2, "archive-B") != CRANKL_OK)
        return 2;

    crankl_cran_t a{};
    crankl_cran_t b{};
    if (crankl_cran_read(path_a, &a) != CRANKL_OK)
        return 3;
    if (crankl_cran_read(path_b, &b) != CRANKL_OK)
        return 4;

    // Both handles must remain usable while the other is open.
    if (crankl_cran_verify(&a) != CRANKL_OK || crankl_cran_verify(&b) != CRANKL_OK)
        return 5;
    if (a.mmap_base == b.mmap_base) {
        std::fprintf(stderr, "FAIL: both handles share one mapping\n");
        return 6;
    }
    if (first_slot(&a) != FILL_A) {
        std::fprintf(stderr, "FAIL: archive A unreadable while B is open\n");
        return 7;
    }
    if (first_slot(&b) != FILL_B) {
        std::fprintf(stderr, "FAIL: archive B unreadable while A is open\n");
        return 8;
    }

    // Metadata reads through mmap_base, so exercise it with both archives open.
    crankl_cran_metadata_t meta{};
    if (crankl_cran_read_metadata(&b, &meta) != CRANKL_OK)
        return 9;
    if (std::strcmp(meta.model_name, "archive-B") != 0) {
        std::fprintf(stderr, "FAIL: metadata model_name '%s'\n", meta.model_name);
        return 10;
    }

    // Closing B must not disturb A, and close must clear the handle it was given.
    crankl_cran_close(&b);
    if (b.mmap_base != nullptr || crankl_cran_verify(&b) == CRANKL_OK)
        return 11;
    if (first_slot(&a) != FILL_A) {
        std::fprintf(stderr, "FAIL: closing B invalidated A\n");
        return 12;
    }
    crankl_cran_close(&a);

    // Close is idempotent, including on a handle that was never read.
    crankl_cran_close(&a);
    crankl_cran_close(&b);
    crankl_cran_t never{};
    crankl_cran_close(&never);

    // Same pair again, closed in the opposite order.
    crankl_cran_t a2{};
    crankl_cran_t b2{};
    if (crankl_cran_read(path_a, &a2) != CRANKL_OK)
        return 13;
    if (crankl_cran_read(path_b, &b2) != CRANKL_OK)
        return 14;
    crankl_cran_close(&a2);
    if (first_slot(&b2) != FILL_B) {
        std::fprintf(stderr, "FAIL: closing A invalidated B\n");
        return 15;
    }
    crankl_cran_close(&b2);

    std::printf("test_cran_multi_open ok a=%016llx b=%016llx\n",
                static_cast<unsigned long long>(FILL_A), static_cast<unsigned long long>(FILL_B));
    return 0;
}
