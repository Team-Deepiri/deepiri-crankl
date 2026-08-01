#pragma once

#include "crankl/crankl.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace crankl {
namespace io {

constexpr char CRANK_MAGIC[6] = {'C', 'R', 'A', 'N', 'K', '\x01'};
constexpr char CRAN_LEGACY_MAGIC[6] = {'C', 'R', 'A', 'N', '\x01', '\0'};

inline bool magic_is_valid(const char *magic) {
    return std::memcmp(magic, CRANK_MAGIC, 6) == 0 || std::memcmp(magic, CRAN_LEGACY_MAGIC, 5) == 0;
}

// FORMAT REDESIGN NOTES
// ---------------------
// The current packed structure below is 124 bytes, not CRAN_HEADER_SIZE (128):
// fields through checksum occupy 36 bytes and reserved[88] brings the total to 124.
// Existing v1/v2 files therefore begin their slot payload at byte 124. Do not simply
// enlarge reserved[]: doing that without version-aware parsing would shift the payload
// and make every existing archive unreadable.
//
// The proposed v3 design in docs/CRANK_FORMAT.md uses a real 128-byte header and
// explicit section flags:
//   bit 0: metadata section is present
//   bit 1: layer-stack section is present
// A v3 reader must choose the payload offset from the on-disk version (124 for v1/v2,
// 128 for v3). A v3 writer should derive flags from sections it actually writes.
//
// TODO(format-v3): introduce separate legacy and v3 disk-header structures, add
// static_asserts for both sizes, and stop using sizeof(one struct) for all versions.
constexpr size_t CRAN_HEADER_SIZE = 128;
constexpr size_t CRAN_SLOT_ENTRY = 16;

#pragma pack(push, 1)
struct CranHeaderDisk {
    char magic[6];
    uint16_t version;
    uint64_t n_slots;
    uint32_t depth_max;
    float gamma;
    uint32_t flags;
    uint64_t checksum;
    uint8_t reserved[88];
};
#pragma pack(pop)

struct CranMetadata {
    char model_name[128];
    char source_hash[64];
};

static constexpr uint32_t FOOTER_MAGIC = 0x4D455441u; // META

// Hard caps to reject malformed or hostile inputs before mmap / large allocations.
constexpr size_t CRANKL_MAX_FILE_BYTES = 1u << 30;         // 1 GiB
constexpr size_t CRANKL_MAX_FLOAT_BYTES = 256u << 20;      // 256 MiB of f32 payload
constexpr uint64_t CRANKL_MAX_SLOTS = 1u << 20;            // 1M crank slots
constexpr uint32_t CRANKL_MAX_STACK_LAYERS = 1u << 16;     // 64k finetune snapshots
constexpr size_t CRANKL_MAX_SAFETENSORS_HEADER = 1u << 24; // 16 MiB JSON header
constexpr size_t CRANKL_MAX_TENSOR_BYTES = 512u << 20;     // 512 MiB per tensor slice

inline bool size_mul_overflow(uint64_t a, uint64_t b, uint64_t &out) {
    if (a == 0 || b == 0) {
        out = 0;
        return false;
    }
    if (a > UINT64_MAX / b)
        return true;
    out = a * b;
    return false;
}

int write_cran(const char *path, const crankl_cran_header_t *hdr, const uint64_t *slots,
               const uint64_t *layer_stacks, const uint32_t *depths);
int write_cran_with_metadata(const char *path, const crankl_cran_header_t *hdr,
                             const uint64_t *slots, const CranMetadata *meta);
int read_cran(const char *path, crankl_cran_t *out);
void close_cran(crankl_cran_t *cran);
int verify_cran(const crankl_cran_t *cran);
int read_cran_metadata(const crankl_cran_t *cran, CranMetadata *out);

} // namespace io
} // namespace crankl
