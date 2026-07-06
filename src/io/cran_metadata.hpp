#pragma once

#include "crankle/crankle.h"

#include <cstddef>
#include <cstdint>

namespace crankle {
namespace io {

struct CranMetadata {
    char model_name[128];
    char source_hash[64];
};

int write_cran_with_metadata(const char *path, const ::crankle_cran_header_t *hdr,
                             const uint64_t *slots, const CranMetadata *meta);
int read_cran_metadata(const ::crankle_cran_t *cran, CranMetadata *out);

} // namespace io
} // namespace crankle
