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

// Slots start at the 124-byte packed header, so they are only 4-byte aligned. memcpy alone
// is not enough: -fsanitize=alignment inspects the source *type*, so passing the
// `const uint64_t *` still reports "load of misaligned address ... requires 8 byte
// alignment" (it recovers, so it does not fail the build -- run with
// -fno-sanitize-recover=alignment to see it). Go through unsigned char, which has
// alignment 1, so no over-aligned pointer is ever formed.
static uint64_t first_slot(const crankl_cran_t *cran) {
    uint64_t w = 0;
    std::memcpy(&w, reinterpret_cast<const unsigned char *>(cran->slots), sizeof(w));
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

    // Reload in place: reading into a live handle must release the old mapping and install
    // the new one, not strand the old one or free the new one.
    //
    // What this loop does NOT prove: that the old mapping was released. LeakSanitizer
    // accounts for allocator calls, not mmap regions, so deleting the release from
    // read_cran entirely leaves this green under the CI sanitizer job. It checks that the
    // handle still reads the right archive after each swap. The size-changing loop below
    // is what fails on a wrong release, because that one corrupts rather than leaks.
    crankl_cran_t reload{};
    if (crankl_cran_read(path_a, &reload) != CRANKL_OK)
        return 16;
    for (int i = 0; i < 64; ++i) {
        const char *next = (i % 2 == 0) ? path_b : path_a;
        if (crankl_cran_read(next, &reload) != CRANKL_OK) {
            std::fprintf(stderr, "FAIL: reload %d rejected\n", i);
            return 17;
        }
        const uint64_t want = (i % 2 == 0) ? FILL_B : FILL_A;
        if (first_slot(&reload) != want) {
            std::fprintf(stderr, "FAIL: reload %d read stale or freed data\n", i);
            return 18;
        }
    }
    crankl_cran_close(&reload);
    if (reload.mmap_base != nullptr || crankl_cran_verify(&reload) == CRANKL_OK) {
        std::fprintf(stderr, "FAIL: reloaded handle survived close\n");
        return 19;
    }

    // Reload across a large size difference, in both directions. munmap needs the length
    // the mapping was created with, so releasing the outgoing mapping must use the size
    // captured before the handle's fields are overwritten. Pairing the old base with the
    // new size unmaps past the end of the old mapping when growing -- taking out whatever
    // is mapped next -- and releases only part of it when shrinking. The archives above sit
    // within one page of each other, so they cannot expose either failure; these differ by
    // hundreds of KB.
    const char *path_big = "/tmp/crankl_multi_big.crank";
    const char *path_tiny = "/tmp/crankl_multi_tiny.crank";
    if (write_filled(path_big, FILL_A, 60000, nullptr) != CRANKL_OK)
        return 20;
    if (write_filled(path_tiny, FILL_B, 8, nullptr) != CRANKL_OK)
        return 21;

    crankl_cran_t sized{};
    if (crankl_cran_read(path_big, &sized) != CRANKL_OK)
        return 22;
    for (int i = 0; i < 256; ++i) {
        const int want_big = (i % 2 == 1);
        if (crankl_cran_read(want_big ? path_big : path_tiny, &sized) != CRANKL_OK) {
            std::fprintf(stderr, "FAIL: size-changing reload %d rejected\n", i);
            return 23;
        }
        if (first_slot(&sized) != (want_big ? FILL_A : FILL_B)) {
            std::fprintf(stderr, "FAIL: size-changing reload %d read stale or freed data\n", i);
            return 24;
        }
        if (sized.header.n_slots != (want_big ? 60000u : 8u)) {
            std::fprintf(stderr, "FAIL: size-changing reload %d has the wrong slot count\n", i);
            return 25;
        }
    }
    crankl_cran_close(&sized);
    std::remove(path_big);
    std::remove(path_tiny);

    std::printf("test_cran_multi_open ok a=%016llx b=%016llx\n",
                static_cast<unsigned long long>(FILL_A), static_cast<unsigned long long>(FILL_B));
    return 0;
}
