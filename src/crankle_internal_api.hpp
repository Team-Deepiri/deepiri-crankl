#pragma once

#include "crankle/crankle.h"
#include "io/cran_metadata.hpp"

namespace crankle {
namespace io {

int write_cran(const char *path, const crankle_cran_header_t *hdr, const uint64_t *slots,
               const uint64_t *layer_stacks, const uint32_t *depths);
int write_cran_with_metadata(const char *path, const crankle_cran_header_t *hdr,
                             const uint64_t *slots, const CranMetadata *meta);
int read_cran(const char *path, crankle_cran_t *out);
void close_cran(crankle_cran_t *cran);
int verify_cran(const crankle_cran_t *cran);
int read_cran_metadata(const crankle_cran_t *cran, CranMetadata *out);

} // namespace io

namespace pack {

int fold_f32(const float *data, size_t count, uint64_t *out_slots, size_t n_slots, float lambda,
             float mu);
int unfold_f32(const uint64_t *slots, size_t n_slots, float *out, size_t count);

} // namespace pack

namespace holonomy {

int forward(const crankle_cran_t *cran, const float *x, size_t dim, float *y);

} // namespace holonomy
} // namespace crankle
