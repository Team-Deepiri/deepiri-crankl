#include "io/safetensors.hpp"
#include "io/security_limits.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

namespace crankle {
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

    size_t shape_pos = block.find("\"shape\":[");
    if (shape_pos != std::string::npos) {
        shape_pos += 9;
        size_t shape_end = block.find(']', shape_pos);
        std::string shape_str = block.substr(shape_pos, shape_end - shape_pos);
        t.shape.clear();
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
            t.shape.push_back(v);
            i = static_cast<size_t>(end - shape_str.c_str());
        }
    }

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

int read_safetensors_f32(const char *path, const char *tensor_name, std::vector<float> &out,
                         SafetensorsTensor *meta) {
    if (!path || !tensor_name)
        return -1;

    std::ifstream f(path, std::ios::binary);
    if (!f)
        return -2;

    uint64_t header_len = 0;
    f.read(reinterpret_cast<char *>(&header_len), 8);
    if (!f || header_len == 0 || header_len > CRANKLE_MAX_SAFETENSORS_HEADER)
        return -3;

    std::string header(header_len, '\0');
    f.read(header.data(), static_cast<std::streamsize>(header_len));
    if (!f)
        return -4;

    SafetensorsTensor tensor;
    if (!parse_tensor_block(header, tensor_name, tensor))
        return -5;
    if (tensor.byte_len == 0 || tensor.byte_len > CRANKLE_MAX_TENSOR_BYTES ||
        tensor.byte_len % sizeof(float) != 0)
        return -8;

    size_t data_base = 8 + header_len;
    if (tensor.byte_offset > CRANKLE_MAX_FILE_BYTES ||
        data_base + tensor.byte_offset > CRANKLE_MAX_FILE_BYTES ||
        data_base + tensor.byte_offset + tensor.byte_len > CRANKLE_MAX_FILE_BYTES)
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
} // namespace crankle
