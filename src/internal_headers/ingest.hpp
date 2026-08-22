#pragma once

#include "crankl/types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace crankl {
namespace io {

struct SafetensorsTensor {
    std::string name;
    std::string dtype;
    std::vector<int64_t> shape;
    size_t byte_offset = 0;
    size_t byte_len = 0;
};

// Load one F32 tensor by name from a safetensors file.
int read_safetensors_f32(const char *path, const char *tensor_name, std::vector<float> &out,
                         SafetensorsTensor *meta = nullptr);

// Enumerate every tensor in a safetensors file (any dtype). Bounds-checked.
int enumerate_safetensors_tensors(const char *path, std::vector<SafetensorsTensor> &out);

// GGUF (v1-v3) ingest. Only F32 and F16 tensors are readable; F16 is widened to F32.
int enumerate_gguf_tensors(const char *path, std::vector<SafetensorsTensor> &out);
int read_gguf_f32(const char *path, const char *tensor_name, std::vector<float> &out,
                  SafetensorsTensor *meta = nullptr);

// One entry of the multi-tensor index stored in an archive META footer.
struct ArchiveTensorEntry {
    std::string name;
    uint64_t slot_offset = 0;
    uint64_t n_slots = 0;
    uint64_t n_floats = 0;
    std::string checksum; // xxh64 hex of the original f32 bytes
};

// Parse the tensor index out of a mapped archive. Returns 0 when an index exists.
int read_archive_tensor_index(const ::crankl_cran_t *cran, std::vector<ArchiveTensorEntry> &out);

// Pack every F32 tensor of a safetensors file into ONE archive whose META footer
// carries the per-tensor slot ranges + checksums. Optionally emit a manifest v2 JSON.
constexpr size_t CRANKL_MAX_TENSORS_PER_ARCHIVE = 32;

int pack_safetensors_multi(const char *path, const char *output_crank, const char *manifest_path,
                           float alpha, float beta);

} // namespace io
} // namespace crankl
