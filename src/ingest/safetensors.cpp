#include "internal_headers/archive.hpp"
#include "internal_headers/ingest.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

namespace crankl {
namespace io {

static bool parse_json_string_field(const std::string &json, const std::string &key,
                                    std::string &out) {
    const std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos)
        return false;
    pos = json.find(':', pos);
    if (pos == std::string::npos)
        return false;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
        ++pos;
    if (pos >= json.size() || json[pos] != '"')
        return false;
    ++pos;
    size_t end = json.find('"', pos);
    if (end == std::string::npos)
        return false;
    out = json.substr(pos, end - pos);
    return true;
}

static bool parse_tensor_shape(const std::string &block, std::vector<int64_t> &shape) {
    size_t shape_pos = block.find("\"shape\":[");
    if (shape_pos == std::string::npos)
        return false;
    shape_pos += 9;
    size_t shape_end = block.find(']', shape_pos);
    if (shape_end == std::string::npos)
        return false;
    std::string shape_str = block.substr(shape_pos, shape_end - shape_pos);
    shape.clear();
    size_t i = 0;
    while (i < shape_str.size()) {
        while (i < shape_str.size() && (shape_str[i] == ' ' || shape_str[i] == ','))
            ++i;
        if (i >= shape_str.size())
            break;
        char *end = nullptr;
        long v = std::strtol(shape_str.c_str() + i, &end, 10);
        if (end == shape_str.c_str() + i)
            break;
        shape.push_back(v);
        i = static_cast<size_t>(end - shape_str.c_str());
    }
    return true;
}

static bool parse_tensor_block(const std::string &json, const std::string &tensor_name,
                               SafetensorsTensor &t) {
    const std::string key = "\"" + tensor_name + "\"";
    size_t pos = json.find(key);
    if (pos == std::string::npos)
        return false;
    size_t block_end = json.find('}', pos);
    if (block_end == std::string::npos)
        return false;
    std::string block = json.substr(pos, block_end - pos);

    std::string dtype;
    if (!parse_json_string_field(block, "dtype", dtype) || dtype != "F32")
        return false;
    t.dtype = dtype;

    parse_tensor_shape(block, t.shape);

    size_t off_pos = block.find("\"data_offsets\":[");
    if (off_pos == std::string::npos)
        return false;
    off_pos += 16;
    long start = 0, end_off = 0;
    if (std::sscanf(block.c_str() + off_pos, "%ld,%ld", &start, &end_off) != 2)
        return false;
    t.byte_offset = static_cast<size_t>(start);
    t.byte_len = static_cast<size_t>(end_off - start);
    t.name = tensor_name;
    return true;
}

// Scan the header JSON for top-level tensor keys. A tensor key is a quoted string
// followed by an object containing "dtype" and "data_offsets". The header has no
// nested objects except per-tensor ones, and "__metadata__" is skipped explicitly.
static bool scan_tensor_key_at(const std::string &json, size_t key_start, std::string &name_out) {
    size_t pos = json.find('"', key_start);
    if (pos == std::string::npos)
        return false;
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos)
        return false;
    name_out = json.substr(pos + 1, end - pos - 1);
    return true;
}

int enumerate_safetensors_tensors(const char *path, std::vector<SafetensorsTensor> &out) {
    out.clear();
    if (!path)
        return -1;

    std::ifstream f(path, std::ios::binary);
    if (!f)
        return -2;

    uint64_t header_len = 0;
    f.read(reinterpret_cast<char *>(&header_len), 8);
    if (!f || header_len == 0 || header_len > CRANKL_MAX_SAFETENSORS_HEADER)
        return -3;

    std::string header(header_len, '\0');
    f.read(header.data(), static_cast<std::streamsize>(header_len));
    if (!f)
        return -4;

    size_t data_base = 8 + header_len;
    size_t i = 0;
    while (i < header.size()) {
        size_t quote = header.find('"', i);
        if (quote == std::string::npos)
            break;
        size_t key_end = header.find('"', quote + 1);
        if (key_end == std::string::npos)
            break;
        std::string name = header.substr(quote + 1, key_end - quote - 1);

        // Advance past the key to find the value object.
        size_t colon = header.find(':', key_end + 1);
        if (colon == std::string::npos)
            break;
        if (name == "__metadata__" || header[colon + 1] != '{') {
            i = colon + 1;
            continue;
        }

        size_t obj_end = header.find('}', colon);
        if (obj_end == std::string::npos)
            break;
        std::string block = header.substr(colon, obj_end - colon);

        SafetensorsTensor t;
        t.name = name;
        parse_json_string_field(block, "dtype", t.dtype);
        parse_tensor_shape(block, t.shape);

        size_t off_pos = block.find("\"data_offsets\":[");
        if (off_pos == std::string::npos) {
            i = obj_end + 1;
            continue;
        }
        off_pos += 16;
        long start = 0, end_off = 0;
        if (std::sscanf(block.c_str() + off_pos, "%ld,%ld", &start, &end_off) != 2 || start < 0 ||
            end_off < start) {
            i = obj_end + 1;
            continue;
        }
        t.byte_offset = static_cast<size_t>(start);
        t.byte_len = static_cast<size_t>(end_off - start);

        if (t.byte_offset > CRANKL_MAX_FILE_BYTES ||
            data_base + t.byte_offset > CRANKL_MAX_FILE_BYTES ||
            data_base + t.byte_offset + t.byte_len > CRANKL_MAX_FILE_BYTES)
            return -9;
        out.push_back(std::move(t));
        i = obj_end + 1;
    }
    return 0;
}

int read_safetensors_f32(const char *path, const char *tensor_name, std::vector<float> &out,
                         SafetensorsTensor *meta) {
    if (!path || !tensor_name)
        return -1;

    std::ifstream f(path, std::ios::binary);
    if (!f)
        return -2;

    uint64_t header_len = 0;
    f.read(reinterpret_cast<char *>(&header_len), 8);
    if (!f || header_len == 0 || header_len > CRANKL_MAX_SAFETENSORS_HEADER)
        return -3;

    std::string header(header_len, '\0');
    f.read(header.data(), static_cast<std::streamsize>(header_len));
    if (!f)
        return -4;

    SafetensorsTensor tensor;
    if (!parse_tensor_block(header, tensor_name, tensor))
        return -5;
    if (tensor.byte_len == 0 || tensor.byte_len > CRANKL_MAX_TENSOR_BYTES ||
        tensor.byte_len % sizeof(float) != 0)
        return -8;

    size_t data_base = 8 + header_len;
    if (tensor.byte_offset > CRANKL_MAX_FILE_BYTES ||
        data_base + tensor.byte_offset > CRANKL_MAX_FILE_BYTES ||
        data_base + tensor.byte_offset + tensor.byte_len > CRANKL_MAX_FILE_BYTES)
        return -9;
    f.seekg(static_cast<std::streamoff>(data_base + tensor.byte_offset));
    if (!f)
        return -6;

    size_t n_floats = tensor.byte_len / sizeof(float);
    out.resize(n_floats);
    f.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(tensor.byte_len));
    if (!f)
        return -7;

    if (meta)
        *meta = tensor;
    return 0;
}

} // namespace io
} // namespace crankl
