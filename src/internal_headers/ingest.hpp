#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace crankl {
namespace io {

struct SafetensorsTensor {
    std::string name;
    std::vector<int64_t> shape;
    size_t byte_offset = 0;
    size_t byte_len = 0;
};

// Load one F32 tensor by name from a safetensors file.
int read_safetensors_f32(const char *path, const char *tensor_name, std::vector<float> &out,
                         SafetensorsTensor *meta = nullptr);

} // namespace io
} // namespace crankl
