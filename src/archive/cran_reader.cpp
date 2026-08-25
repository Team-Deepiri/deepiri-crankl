#include "crankl/crankl.h"
#include "internal_headers/archive.hpp"
#include "xxhash.h"

#include <cstring>
#include <string>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------
// Portability layer: the reader maps the archive read-only and hands the base
// pointer + length to the public handle. POSIX uses open/fstat/mmap; Windows
// uses CreateFileW/CreateFileMappingW/MapViewOfFile. Both paths yield the same
// contract: base is valid until close_cran, which releases it by pointer
// (munmap needs base+size, UnmapViewOfFile needs only base).
// ---------------------------------------------------------------------------

#if defined(_WIN32)

namespace {

struct MappedFile {
    void *base = nullptr;
    size_t size = 0;
};

// Returns 0 and fills mf on success; mirrors the POSIX error codes below.
int map_file_read(const char *path, MappedFile &mf) {
    if (!path)
        return -1;
    const int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    if (wlen <= 0)
        return -2;
    std::wstring wpath(static_cast<size_t>(wlen), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, &wpath[0], wlen) != wlen)
        return -2;

    HANDLE file = CreateFileW(wpath.c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return -2;
    LARGE_INTEGER li{};
    if (!GetFileSizeEx(file, &li) || li.QuadPart < 0) {
        CloseHandle(file);
        return -3;
    }
    if (static_cast<uint64_t>(li.QuadPart) > CRANKL_MAX_FILE_BYTES) {
        CloseHandle(file);
        return -14;
    }
    if (li.QuadPart == 0) {
        CloseHandle(file);
        return -4;
    }
    // Size-zero sections are rejected above; 0/0 maps the entire current file.
    HANDLE section = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    CloseHandle(file);
    if (!section)
        return -4;
    void *base = MapViewOfFile(section, FILE_MAP_READ, 0, 0, 0);
    // The view holds its own reference to the section; it stays valid until
    // UnmapViewOfFile even after this handle is closed.
    CloseHandle(section);
    if (!base)
        return -4;
    mf.base = base;
    mf.size = static_cast<size_t>(li.QuadPart);
    return 0;
}

void unmap_file(void *base, size_t /*size*/) {
    if (base)
        UnmapViewOfFile(base);
}

} // namespace

#else // POSIX

namespace {

void unmap_file(void *base, size_t size) {
    if (base)
        munmap(base, size);
}

class UniqueFd {
  public:
    UniqueFd() = default;
    explicit UniqueFd(int fd) : fd_(fd) {}
    UniqueFd(const UniqueFd &) = delete;
    UniqueFd &operator=(const UniqueFd &) = delete;
    UniqueFd(UniqueFd &&other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    UniqueFd &operator=(UniqueFd &&other) noexcept {
        if (this != &other) {
            reset();
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }
    ~UniqueFd() {
        reset();
    }

    void reset() {
        if (fd_ >= 0)
            close(fd_);
        fd_ = -1;
    }
    bool valid() const {
        return fd_ >= 0;
    }
    int get() const {
        return fd_;
    }

  private:
    int fd_ = -1;
};

class MappedRegion {
  public:
    MappedRegion() = default;
    MappedRegion(void *base, size_t size) : base_(base), size_(size) {}
    MappedRegion(const MappedRegion &) = delete;
    MappedRegion &operator=(const MappedRegion &) = delete;
    MappedRegion(MappedRegion &&other) noexcept
        : base_(std::exchange(other.base_, MAP_FAILED)), size_(std::exchange(other.size_, 0)) {}
    MappedRegion &operator=(MappedRegion &&other) noexcept {
        if (this != &other) {
            reset();
            base_ = std::exchange(other.base_, MAP_FAILED);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }
    ~MappedRegion() {
        reset();
    }

    void reset() {
        if (base_ != MAP_FAILED)
            munmap(base_, size_);
        base_ = MAP_FAILED;
        size_ = 0;
    }
    // Hands the mapping to a crankl_cran_t, which owns it until close_cran.
    void *release() {
        void *base = base_;
        base_ = MAP_FAILED;
        size_ = 0;
        return base;
    }

  private:
    void *base_ = MAP_FAILED;
    size_t size_ = 0;
};

} // namespace

#endif // _WIN32

namespace crankl {
namespace io {

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
#if defined(_WIN32)
    MappedFile mf{};
    const int map_rc = map_file_read(path, mf);
    if (map_rc != 0)
        return map_rc;
    void *base = mf.base;
    size_t sz = mf.size;
#else
    UniqueFd fd(open(path, O_RDONLY));
    if (!fd.valid())
        return -2;
    struct stat st {};
    if (fstat(fd.get(), &st) != 0)
        return -3;
    if (st.st_size < 0 || static_cast<uint64_t>(st.st_size) > CRANKL_MAX_FILE_BYTES)
        return -14;
    size_t sz = static_cast<size_t>(st.st_size);
    void *base = mmap(nullptr, sz, PROT_READ, MAP_PRIVATE, fd.get(), 0);
    if (base == MAP_FAILED)
        return -4;
    MappedRegion map(base, sz);
    // mmap(2) gives the mapping its own reference to the file, so the descriptor is no
    // longer needed once the mapping exists. Releasing it here is what allows a handle to
    // own its mapping using only the mmap_base/mmap_size fields it already publishes.
    fd.reset();
#endif
    if (sz < sizeof(CranHeaderDisk))
        return -5;
    auto *hd = static_cast<CranHeaderDisk *>(base);
    if (!magic_is_valid(hd->magic))
        return -6;
    const uint8_t *payload = reinterpret_cast<const uint8_t *>(base) + sizeof(CranHeaderDisk);
    size_t payload_len = sz - sizeof(CranHeaderDisk);

    const uint8_t *layers_ptr = nullptr;
    int layout_rc = validate_cran_layout(hd, payload_len, layers_ptr);
    if (layout_rc != 0)
        return layout_rc;

    uint64_t chk = crankl_xxhash64(payload, payload_len, 0);
    if (chk != hd->checksum)
        return -7;

    // Capture the outgoing mapping before any field is overwritten. munmap needs the
    // base paired with the length that mapping was created with, and out->mmap_size is
    // about to become the new archive's size. Pairing the old base with the new size
    // unmaps the wrong range: past the end of the old mapping when the new archive is
    // larger, and only part of it when smaller.
    void *old_base = out->mmap_base;
    size_t old_size = out->mmap_size;

    out->mmap_size = sz;
    out->header.n_slots = hd->n_slots;
    out->header.depth_max = hd->depth_max;
    out->header.gamma = hd->gamma;
    out->header.flags = hd->flags;
    out->slots = reinterpret_cast<const uint64_t *>(payload);
    // layers is non-null only when a validated stack section exists. There is no
    // address fallback: pointing at the bytes after slots would hand callers a
    // pointer that may alias the META footer or EOF, which peel would then read
    // as history. NULL is the "no validated history" sentinel, matching close.
    out->layers = layers_ptr ? reinterpret_cast<const uint64_t *>(layers_ptr) : nullptr;
    // Each handle owns its own mapping, so any number of archives may be open at once.
    // Ownership moves to the caller last, once every other field is valid; close_cran
    // keys off mmap_base.
    //
    // Release a mapping the handle was already holding first. Without this, reloading in
    // place -- read into the same handle without closing it -- would strand the previous
    // mapping, which is the one lifetime pattern the old global happened to get right.
    // Only reached on a handle that already went through read_cran, since the contract
    // requires a zero-initialised or closed handle.
    if (old_base)
        unmap_file(old_base, old_size);
#ifndef _WIN32
    out->mmap_base = map.release();
#else
    out->mmap_base = base;
#endif
    return 0;
}

void close_cran(::crankl_cran_t *cran) {
    if (!cran)
        return;
    if (cran->mmap_base)
        unmap_file(cran->mmap_base, cran->mmap_size);
    // Clearing the view makes a second close a no-op and turns any later use of a closed
    // handle into a null dereference instead of a read through a dangling pointer.
    cran->mmap_base = nullptr;
    cran->mmap_size = 0;
    cran->slots = nullptr;
    cran->layers = nullptr;
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
