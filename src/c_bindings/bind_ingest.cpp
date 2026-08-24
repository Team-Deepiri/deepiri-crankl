#include "crankl/ingest.h"

#include "internal_headers/archive.hpp"
#include "internal_headers/c_bindings.hpp"
#include "internal_headers/ingest.hpp"

#include <cstring>

#include <cstdlib>

extern "C" {

static int fill_source_list(const std::vector<crankl::io::SafetensorsTensor> &tensors,
                            crankl_source_tensor_t *out, size_t capacity) {
    if (!out || capacity < tensors.size())
        return CRANKL_ERR_INVALID;
    for (size_t i = 0; i < tensors.size(); ++i) {
        std::memset(&out[i], 0, sizeof(out[i]));
        std::strncpy(out[i].name, tensors[i].name.c_str(), CRANKL_INGEST_NAME_MAX - 1);
        out[i].n_floats = tensors[i].byte_len / ((tensors[i].dtype == "F16") ? 2 : 4);
        out[i].is_f32 = tensors[i].dtype == "F32" ? 1u : 0u;
    }
    return CRANKL_OK;
}

int crankl_safetensors_count(const char *path, size_t *out_count) {
    if (!path || !out_count)
        return CRANKL_ERR_NULL;
    std::vector<crankl::io::SafetensorsTensor> tensors;
    if (crankl::io::enumerate_safetensors_tensors(path, tensors) != 0)
        return CRANKL_ERR_IO;
    *out_count = tensors.size();
    return CRANKL_OK;
}

int crankl_safetensors_list(const char *path, crankl_source_tensor_t *out, size_t capacity) {
    if (!path)
        return CRANKL_ERR_NULL;
    std::vector<crankl::io::SafetensorsTensor> tensors;
    if (crankl::io::enumerate_safetensors_tensors(path, tensors) != 0)
        return CRANKL_ERR_IO;
    return fill_source_list(tensors, out, capacity);
}

int crankl_gguf_count(const char *path, size_t *out_count) {
    if (!path || !out_count)
        return CRANKL_ERR_NULL;
    std::vector<crankl::io::SafetensorsTensor> tensors;
    if (crankl::io::enumerate_gguf_tensors(path, tensors) != 0)
        return CRANKL_ERR_IO;
    *out_count = tensors.size();
    return CRANKL_OK;
}

int crankl_gguf_list(const char *path, crankl_source_tensor_t *out, size_t capacity) {
    if (!path)
        return CRANKL_ERR_NULL;
    std::vector<crankl::io::SafetensorsTensor> tensors;
    if (crankl::io::enumerate_gguf_tensors(path, tensors) != 0)
        return CRANKL_ERR_IO;
    return fill_source_list(tensors, out, capacity);
}

static int pack_one(const std::vector<float> &data, const char *output_crank, float alpha,
                    float beta) {
    if (data.empty())
        return CRANKL_ERR_INVALID;
    size_t n_slots = crankl_pack_n_slots(data.size());
    std::vector<uint64_t> slots(n_slots);
    if (crankl_pack_f32(data.data(), data.size(), slots.data(), n_slots, alpha, beta) != 0)
        return CRANKL_ERR_INVALID;
    crankl_cran_header_t hdr{};
    hdr.n_slots = n_slots;
    hdr.depth_max = 1;
    hdr.gamma = 1.0f;
    return crankl_cran_write(output_crank, &hdr, slots.data(), nullptr, nullptr);
}

int crankl_pack_safetensors_multi(const char *path, const char *output_crank,
                                  const char *manifest_path, float alpha, float beta) {
    if (!path || !output_crank)
        return CRANKL_ERR_NULL;
    int rc = crankl::io::pack_safetensors_multi(path, output_crank, manifest_path, alpha, beta);
    if (rc == -4)
        return CRANKL_ERR_IO;
    return rc == 0 ? CRANKL_OK : CRANKL_ERR_INVALID;
}

int crankl_pack_safetensors_tensor(const char *path, const char *tensor_name,
                                   const char *output_crank) {
    if (!path || !tensor_name || !output_crank)
        return CRANKL_ERR_NULL;
    std::vector<float> data;
    if (crankl::io::read_safetensors_f32(path, tensor_name, data) != 0)
        return CRANKL_ERR_IO;
    return pack_one(data, output_crank, 0.1f, 0.01f);
}

int crankl_safetensors_read_f32(const char *path, const char *tensor_name, float **out,
                                size_t *n_floats) {
    if (!path || !tensor_name || !out || !n_floats)
        return CRANKL_ERR_NULL;
    *out = nullptr;
    *n_floats = 0;
    std::vector<float> data;
    if (crankl::io::read_safetensors_f32(path, tensor_name, data) != 0)
        return CRANKL_ERR_IO;
    if (data.empty())
        return CRANKL_ERR_FORMAT;
    float *buf = static_cast<float *>(malloc(data.size() * sizeof(float)));
    if (!buf)
        return CRANKL_ERR_IO;
    std::memcpy(buf, data.data(), data.size() * sizeof(float));
    *out = buf;
    *n_floats = data.size();
    return CRANKL_OK;
}

int crankl_pack_gguf_f32(const char *path, const char *tensor_name, const char *output_crank) {
    if (!path || !tensor_name || !output_crank)
        return CRANKL_ERR_NULL;
    std::vector<float> data;
    if (crankl::io::read_gguf_f32(path, tensor_name, data) != 0)
        return CRANKL_ERR_IO;
    return pack_one(data, output_crank, 0.1f, 0.01f);
}

int crankl_archive_tensor_count(const char *path, size_t *out_count) {
    if (!path || !out_count)
        return CRANKL_ERR_NULL;
    crankl_cran_t cran{};
    if (crankl_cran_read(path, &cran) != 0)
        return CRANKL_ERR_IO;
    std::vector<crankl::io::ArchiveTensorEntry> entries;
    int rc = crankl::io::read_archive_tensor_index(&cran, entries);
    crankl_cran_close(&cran);
    if (rc != 0)
        return rc == 1 ? CRANKL_ERR_NO_METADATA : CRANKL_ERR_FORMAT;
    *out_count = entries.size();
    return CRANKL_OK;
}

int crankl_archive_tensor_list(const char *path, crankl_archive_tensor_t *out, size_t capacity) {
    if (!path)
        return CRANKL_ERR_NULL;
    crankl_cran_t cran{};
    if (crankl_cran_read(path, &cran) != 0)
        return CRANKL_ERR_IO;
    std::vector<crankl::io::ArchiveTensorEntry> entries;
    int rc = crankl::io::read_archive_tensor_index(&cran, entries);
    crankl_cran_close(&cran);
    if (rc != 0)
        return rc == 1 ? CRANKL_ERR_NO_METADATA : CRANKL_ERR_FORMAT;
    if (!out || capacity < entries.size())
        return CRANKL_ERR_INVALID;
    for (size_t i = 0; i < entries.size(); ++i) {
        std::memset(&out[i], 0, sizeof(out[i]));
        std::strncpy(out[i].name, entries[i].name.c_str(), CRANKL_INGEST_NAME_MAX - 1);
        out[i].slot_offset = entries[i].slot_offset;
        out[i].n_slots = entries[i].n_slots;
        out[i].n_floats = entries[i].n_floats;
        std::strncpy(out[i].checksum, entries[i].checksum.c_str(), CRANKL_INGEST_CHECKSUM_MAX - 1);
    }
    return CRANKL_OK;
}

} // extern "C"
