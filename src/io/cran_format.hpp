#pragma once
#include <cstddef>
#include <cstdint>

namespace crankle {
namespace io {

constexpr char CRAN_MAGIC[6] = {'C', 'R', 'A', 'N', '\x01', '\0'};
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
} // namespace crankle
