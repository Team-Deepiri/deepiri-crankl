#pragma once

#include "crankl/crankl.h"

#include <cstddef>
#include <cstdint>

namespace crankl {
namespace io {

struct CranMetadata {
    char model_name[128];
    char source_hash[64];
};

static constexpr uint32_t FOOTER_MAGIC = 0x4D455441u; // META

int write_cran_with_metadata(const char *path, const ::crankl_cran_header_t *hdr,
                             const uint64_t *slots, const CranMetadata *meta);
int read_cran_metadata(const ::crankl_cran_t *cran, CranMetadata *out);

} // namespace io
} // namespace crankl
