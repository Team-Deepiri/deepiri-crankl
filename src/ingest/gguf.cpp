#include "internal_headers/archive.hpp"
#include "internal_headers/ingest.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace crankl {
namespace io {

// Minimal GGUF (v1-v3) reader. The container is a flat header:
//   magic u32 | version u32 | tensor_count u64 | kv_count u64
//   kv* (string key, u32 type, typed value)
//   tensor infos* (string name, u32 n_dims, u64 dims[], u32 dtype, u64 offset)
// Tensor offsets are relative to the data section, which starts at the first
// multiple of the alignment after the infos. Only F32/F16 tensors are readable.
namespace {

constexpr uint32_t GGUF_MAGIC = 0x46554747u; // "GGUF" little-endian
constexpr size_t GGUF_MAX_HEADER = 1u << 26; // 64 MiB of header/infos
constexpr uint64_t GGUF_MAX_TENSORS = 1u << 20;
constexpr uint64_t GGUF_MAX_KV = 1u << 20;
constexpr uint32_t GGUF_DEFAULT_ALIGNMENT = 32;

enum GgufValueType : uint32_t {
    GGUF_KV_U8 = 0,
    GGUF_KV_I8,
    GGUF_KV_U16,
    GGUF_KV_I16,
    GGUF_KV_U32,
    GGUF_KV_I32,
    GGUF_KV_F32,
    GGUF_KV_BOOL,
    GGUF_KV_STRING,
    GGUF_KV_ARRAY,
    GGUF_KV_U64,
    GGUF_KV_I64,
    GGUF_KV_F64
};

enum GgufDtype : uint32_t { GGUF_F32 = 0, GGUF_F16 = 1 };

struct Cursor {
    const uint8_t *p;
    size_t left;
    bool ok() const {
        return p != nullptr;
    }
};

bool take(Cursor &c, void *dst, size_t n) {
    if (!c.ok() || c.left < n)
        return false;
    std::memcpy(dst, c.p, n);
    c.p += n;
    c.left -= n;
    return true;
}

bool take_u32(Cursor &c, uint32_t &v) {
    return take(c, &v, sizeof v);
}
bool take_u64(Cursor &c, uint64_t &v) {
    return take(c, &v, sizeof v);
}

bool take_string(Cursor &c, std::string &s) {
    uint64_t len = 0;
    if (!take_u64(c, len) || len > c.left || len > GGUF_MAX_HEADER)
        return false;
    s.assign(reinterpret_cast<const char *>(c.p), static_cast<size_t>(len));
    c.p += len;
    c.left -= static_cast<size_t>(len);
    return true;
}

size_t gguf_value_size(uint32_t type) {
    switch (type) {
    case GGUF_KV_U8:
    case GGUF_KV_I8:
    case GGUF_KV_BOOL:
        return 1;
    case GGUF_KV_U16:
    case GGUF_KV_I16:
        return 2;
    case GGUF_KV_U32:
    case GGUF_KV_I32:
    case GGUF_KV_F32:
        return 4;
    case GGUF_KV_U64:
    case GGUF_KV_I64:
    case GGUF_KV_F64:
        return 8;
    default:
        return 0; // string / array handled inline by the caller
    }
}

float half_to_f32(uint16_t h) {
    uint32_t sign = static_cast<uint32_t>(h >> 15) << 31;
    uint32_t exp = (h >> 10) & 0x1Fu;
    uint32_t frac = h & 0x3FFu;
    uint32_t bits;
    if (exp == 0) {
        bits = sign | (frac == 0 ? 0u : (127u - 15u) << 23 | frac << 13);
    } else if (exp == 0x1F) {
        bits = sign | 0xFFu << 23 | frac << 13;
    } else {
        bits = sign | (exp + 127u - 15u) << 23 | frac << 13;
    }
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

} // namespace

int enumerate_gguf_tensors(const char *path, std::vector<SafetensorsTensor> &out) {
    out.clear();
    if (!path)
        return -1;

    std::ifstream f(path, std::ios::binary);
    if (!f)
        return -2;

    f.seekg(0, std::ios::end);
    std::streamoff file_size = f.tellg();
    if (file_size <= 0 || static_cast<size_t>(file_size) > CRANKL_MAX_FILE_BYTES)
        return -9;
    f.seekg(0, std::ios::beg);

    // Only the header/infos section is parsed into memory; tensor payloads stay on disk.
    size_t want = static_cast<size_t>(file_size);
    if (want > GGUF_MAX_HEADER)
        want = GGUF_MAX_HEADER;
    std::vector<uint8_t> header(want);
    f.read(reinterpret_cast<char *>(header.data()), static_cast<std::streamsize>(want));
    if (!f)
        return -4;

    Cursor c{header.data(), header.size()};
    uint32_t magic = 0, version = 0;
    uint64_t tensor_count = 0, kv_count = 0;
    if (!take_u32(c, magic) || magic != GGUF_MAGIC)
        return -5;
    if (!take_u32(c, version) || version < 1 || version > 3)
        return -6;
    if (!take_u64(c, tensor_count) || tensor_count > GGUF_MAX_TENSORS)
        return -7;
    if (!take_u64(c, kv_count) || kv_count > GGUF_MAX_KV)
        return -7;

    uint32_t alignment = GGUF_DEFAULT_ALIGNMENT;
    for (uint64_t i = 0; i < kv_count; ++i) {
        std::string key;
        uint32_t type = 0;
        if (!take_string(c, key) || !take_u32(c, type))
            return -8;
        if (key == "general.alignment" && type == GGUF_KV_U32 && c.left >= 4) {
            std::memcpy(&alignment, c.p, 4);
            c.p += 4;
            c.left -= 4;
            continue;
        }
        if (type == GGUF_KV_ARRAY) {
            uint32_t elem_type = 0;
            uint64_t count = 0;
            if (!take_u32(c, elem_type) || !take_u64(c, count))
                return -8;
            for (uint64_t j = 0; j < count; ++j) {
                if (elem_type == GGUF_KV_STRING) {
                    uint64_t len = 0;
                    if (!take_u64(c, len) || len > c.left || len > GGUF_MAX_HEADER)
                        return -8;
                    c.p += len;
                    c.left -= static_cast<size_t>(len);
                    continue;
                }
                size_t fixed = gguf_value_size(elem_type);
                if (fixed == 0 || fixed > c.left)
                    return -8;
                c.p += fixed;
                c.left -= fixed;
            }
            continue;
        }
        size_t fixed = gguf_value_size(type);
        if (fixed == 0 || fixed > c.left)
            return -8;
        c.p += fixed;
        c.left -= fixed;
    }

    for (uint64_t i = 0; i < tensor_count; ++i) {
        SafetensorsTensor t;
        uint32_t n_dims = 0, dtype = 0;
        uint64_t offset = 0;
        if (!take_string(c, t.name))
            return -8;
        if (!take_u32(c, n_dims) || n_dims == 0 || n_dims > 8)
            return -8;
        uint64_t elems = 1;
        for (uint32_t d = 0; d < n_dims; ++d) {
            uint64_t dim = 0;
            if (!take_u64(c, dim) || dim == 0 || dim > CRANKL_MAX_FLOAT_BYTES)
                return -8;
            t.shape.push_back(static_cast<int64_t>(dim));
            if (elems != 0 && dim != 0 && elems <= UINT64_MAX / dim)
                elems *= dim;
            else
                elems = 0;
        }
        if (!take_u32(c, dtype) || !take_u64(c, offset))
            return -8;
        if (dtype != GGUF_F32 && dtype != GGUF_F16)
            continue; // listed as unpackable later; keep enumeration lossless
        t.dtype = (dtype == GGUF_F32) ? "F32" : "F16";
        size_t elem_size = (dtype == GGUF_F32) ? 4 : 2;
        if (elems == 0 || elems > CRANKL_MAX_TENSOR_BYTES / elem_size)
            return -8;
        t.byte_len = static_cast<size_t>(elems * elem_size);
        t.byte_offset = offset;
        out.push_back(std::move(t));
    }

    size_t header_end = header.size() - c.left;
    if (header_end > header.size())
        return -8;
    size_t data_base = (header_end + alignment - 1) / alignment * alignment;
    const size_t total = static_cast<size_t>(file_size);
    for (auto &t : out) {
        if (data_base > total || t.byte_offset > total - data_base ||
            data_base + t.byte_offset + t.byte_len > total)
            return -9;
        t.byte_offset += data_base;
    }
    return 0;
}

int read_gguf_f32(const char *path, const char *tensor_name, std::vector<float> &out,
                  SafetensorsTensor *meta) {
    if (!path || !tensor_name)
        return -1;

    std::vector<SafetensorsTensor> tensors;
    int rc = enumerate_gguf_tensors(path, tensors);
    if (rc != 0)
        return rc;

    const SafetensorsTensor *found = nullptr;
    for (const auto &t : tensors) {
        if (t.name == tensor_name) {
            found = &t;
            break;
        }
    }
    if (!found)
        return -5;

    std::ifstream f(path, std::ios::binary);
    if (!f)
        return -2;

    size_t n_floats = found->byte_len / ((found->dtype == "F16") ? 2 : 4);
    out.resize(n_floats);

    if (found->dtype == "F32") {
        f.seekg(static_cast<std::streamoff>(found->byte_offset));
        if (!f)
            return -6;
        f.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(found->byte_len));
        if (!f)
            return -7;
    } else {
        std::vector<uint16_t> raw(n_floats);
        f.seekg(static_cast<std::streamoff>(found->byte_offset));
        if (!f)
            return -6;
        f.read(reinterpret_cast<char *>(raw.data()),
               static_cast<std::streamsize>(n_floats * sizeof(uint16_t)));
        if (!f)
            return -7;
        for (size_t i = 0; i < n_floats; ++i)
            out[i] = half_to_f32(raw[i]);
    }

    if (meta)
        *meta = *found;
    return 0;
}

} // namespace io
} // namespace crankl
