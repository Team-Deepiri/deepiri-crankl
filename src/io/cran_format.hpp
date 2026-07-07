#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace crankl {
namespace io {

constexpr char CRANK_MAGIC[6] = {'C', 'R', 'A', 'N', 'K', '\x01'};
constexpr char CRAN_LEGACY_MAGIC[6] = {'C', 'R', 'A', 'N', '\x01', '\0'};

inline bool magic_is_valid(const char *magic) {
    return std::memcmp(magic, CRANK_MAGIC, 6) == 0 ||
           std::memcmp(magic, CRAN_LEGACY_MAGIC, 5) == 0;
}
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

} // namespace io
} // namespace crankl
