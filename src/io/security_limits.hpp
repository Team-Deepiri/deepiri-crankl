#pragma once

#include <cstddef>
#include <cstdint>

namespace crankle {
namespace io {

// Hard caps to reject malformed or hostile inputs before mmap / large allocations.
constexpr size_t CRANKLE_MAX_FILE_BYTES = 1u << 30;          // 1 GiB
constexpr size_t CRANKLE_MAX_FLOAT_BYTES = 256u << 20;        // 256 MiB of f32 payload
constexpr uint64_t CRANKLE_MAX_SLOTS = 1u << 20;               // 1M crank slots
constexpr uint32_t CRANKLE_MAX_STACK_LAYERS = 1u << 16;        // 64k finetune snapshots
constexpr size_t CRANKLE_MAX_SAFETENSORS_HEADER = 1u << 24;  // 16 MiB JSON header
constexpr size_t CRANKLE_MAX_TENSOR_BYTES = 512u << 20;       // 512 MiB per tensor slice

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

} // namespace io
} // namespace crankle
