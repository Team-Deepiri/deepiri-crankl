#include "crankl/crankl.h"
#include "internal_headers/archive.hpp"
#include "xxhash.h"

#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace crankl {
namespace io {

static void *g_mmap_base = MAP_FAILED;
static size_t g_mmap_size = 0;
static int g_mmap_fd = -1;

// LEGACY v1/v2 LAYOUT VALIDATION
// ------------------------------
// Bytes after the current slots are currently ambiguous:
//   flags bit 0 set   => the tail is interpreted as a META footer;
//   otherwise v2     => the tail may be interpreted as a layer stack.
// This means current files cannot contain both sections.
//
// TODO(format-v3): parse explicit STACKS and METADATA flags in a deterministic
// section order. Start layers_out as nullptr, validate every multiplication and
// section bound, advance a cursor after each section, and reject unexplained
// trailing bytes. Return the validated stack count as well as its pointer; callers
// must not use header.depth_max or pointer inequality as proof that history exists.
// Preserve this branch for legacy v1/v2 fixtures. See docs/CRANK_FORMAT.md.
static int validate_cran_layout(const CranHeaderDisk *hd, size_t payload_len,
                                const uint8_t *&layers_out) {
    if (hd->n_slots == 0 || hd->n_slots > CRANKL_MAX_SLOTS)
        return -8;

    uint64_t slots_bytes = 0;
    if (size_mul_overflow(hd->n_slots, 8, slots_bytes) || slots_bytes > payload_len)
        return -9;

    layers_out = nullptr;
    size_t tail_len = payload_len - static_cast<size_t>(slots_bytes);
    const uint8_t *tail = reinterpret_cast<const uint8_t *>(hd) + sizeof(CranHeaderDisk) +
                          static_cast<size_t>(slots_bytes);

    if ((hd->flags & 1u) != 0) {
        if (tail_len < 8)
            return -10;
        uint32_t magic = 0;
        uint32_t json_len = 0;
        std::memcpy(&magic, tail, 4);
        std::memcpy(&json_len, tail + 4, 4);
        if (magic != FOOTER_MAGIC || json_len > 4096)
            return -11;
        if (tail_len < 8 + json_len)
            return -12;
        return 0;
    }

    if (hd->version >= 2 && tail_len >= 4) {
        uint32_t n_stack_layers = 0;
        std::memcpy(&n_stack_layers, tail, 4);
        if (n_stack_layers > CRANKL_MAX_STACK_LAYERS)
            return -13;

        uint64_t stack_word_count = 0;
        if (size_mul_overflow(n_stack_layers, hd->n_slots, stack_word_count))
            return -14;
        uint64_t stack_bytes = 0;
        if (size_mul_overflow(stack_word_count, 8, stack_bytes))
            return -15;
        if (tail_len < 4 + stack_bytes)
            return -16;
        layers_out = tail + 4;
    }
    return 0;
}

int read_cran(const char *path, ::crankl_cran_t *out) {
    if (!path || !out)
        return -1;
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -2;
    struct stat st{};
    if (fstat(fd, &st) != 0) {
        close(fd);
        return -3;
    }
    if (st.st_size < 0 || static_cast<uint64_t>(st.st_size) > CRANKL_MAX_FILE_BYTES) {
        close(fd);
        return -14;
    }
    size_t sz = static_cast<size_t>(st.st_size);
    void *base = mmap(nullptr, sz, PROT_READ, MAP_PRIVATE, fd, 0);
    if (base == MAP_FAILED) {
        close(fd);
        return -4;
    }
    if (sz < sizeof(CranHeaderDisk)) {
        munmap(base, sz);
        close(fd);
        return -5;
    }
    auto *hd = static_cast<CranHeaderDisk *>(base);
    if (!magic_is_valid(hd->magic)) {
        munmap(base, sz);
        close(fd);
        return -6;
    }
    const uint8_t *payload = reinterpret_cast<const uint8_t *>(base) + sizeof(CranHeaderDisk);
    size_t payload_len = sz - sizeof(CranHeaderDisk);

    const uint8_t *layers_ptr = nullptr;
    int layout_rc = validate_cran_layout(hd, payload_len, layers_ptr);
    if (layout_rc != 0) {
        munmap(base, sz);
        close(fd);
        return layout_rc;
    }

    uint64_t chk = crankl_xxhash64(payload, payload_len, 0);
    if (chk != hd->checksum) {
        munmap(base, sz);
        close(fd);
        return -7;
    }

    // TODO(mmap-lifetime): this global ownership model permits only one open
    // archive. Reading archive B unmaps archive A while A's public view still holds
    // dangling pointers. Make each crankl_cran_t own its mapping independently and
    // add a test that keeps two archives open at once.
    if (g_mmap_base != MAP_FAILED) {
        munmap(g_mmap_base, g_mmap_size);
        close(g_mmap_fd);
    }
    g_mmap_base = base;
    g_mmap_size = sz;
    g_mmap_fd = fd;

    out->mmap_base = base;
    out->mmap_size = sz;
    out->header.n_slots = hd->n_slots;
    out->header.depth_max = hd->depth_max;
    out->header.gamma = hd->gamma;
    out->header.flags = hd->flags;
    out->slots = reinterpret_cast<const uint64_t *>(payload);
    // BUG(format-v2): when no stack was validated, this fallback is merely the
    // address after the current slots. It may point at metadata or exactly at EOF.
    // cmd_peel currently treats it as valid history because it is non-null and
    // differs from out->slots. The v3 implementation must use nullptr here and
    // expose an explicit validated n_stack_layers field.
    out->layers =
        layers_ptr ? reinterpret_cast<const uint64_t *>(layers_ptr) : out->slots + hd->n_slots;
    return 0;
}

void close_cran(::crankl_cran_t *cran) {
    (void)cran;
    if (g_mmap_base != MAP_FAILED) {
        munmap(g_mmap_base, g_mmap_size);
        g_mmap_base = MAP_FAILED;
    }
    if (g_mmap_fd >= 0) {
        close(g_mmap_fd);
        g_mmap_fd = -1;
    }
}

int verify_cran(const ::crankl_cran_t *cran) {
    if (!cran || !cran->mmap_base || cran->mmap_size < sizeof(CranHeaderDisk))
        return -1;
    if (cran->header.n_slots == 0 || cran->header.n_slots > CRANKL_MAX_SLOTS)
        return -2;
    return 0;
}

} // namespace io
} // namespace crankl
