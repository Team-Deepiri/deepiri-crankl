#include "crankle/crankle.h"
#include "io/cran_format.hpp"
#include "xxhash.h"

#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace crankle {
namespace io {

static void *g_mmap_base = MAP_FAILED;
static size_t g_mmap_size = 0;
static int g_mmap_fd = -1;

int read_cran(const char *path, ::crankle_cran_t *out) {
    if (!path || !out)
        return -1;
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -2;
    struct stat st {};
    if (fstat(fd, &st) != 0) {
        close(fd);
        return -3;
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
    if (std::memcmp(hd->magic, CRAN_MAGIC, 5) != 0) {
        munmap(base, sz);
        close(fd);
        return -6;
    }
    const uint8_t *payload = reinterpret_cast<const uint8_t *>(base) + sizeof(CranHeaderDisk);
    size_t payload_len = sz - sizeof(CranHeaderDisk);
    uint64_t chk = crankle_xxhash64(payload, payload_len, 0);
    if (chk != hd->checksum) {
        munmap(base, sz);
        close(fd);
        return -7;
    }

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
    out->layers = out->slots + hd->n_slots;
    return 0;
}

void close_cran(::crankle_cran_t *cran) {
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

int verify_cran(const ::crankle_cran_t *cran) {
    if (!cran || !cran->mmap_base)
        return -1;
    return 0;
}

} // namespace io
} // namespace crankle
