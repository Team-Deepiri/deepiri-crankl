#include "crankl/crankl.h"
#include "internal_headers/archive.hpp"
#include "internal_headers/ingest.hpp"
#include "internal_headers/metrics.hpp"
#include "internal_headers/pack.hpp"
#include "crankl/version.h"
#include "xxhash.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

namespace crankl {
namespace io {

namespace {

std::string xxh64_hex(const void *data, size_t len) {
    uint64_t h = crankl_xxhash64(data, len, 0);
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(h));
    return buf;
}

// Escape the two characters JSON forbids inside string literals that can appear
// in tensor names: backslash and double quote. Names come from untrusted files.
std::string json_escape(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        if (ch == '"' || ch == '\\') {
            out.push_back('\\');
            out.push_back(ch);
        } else if (ch == '\n') {
            out += "\\n";
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

void append_tensor_json(std::string &json, const ArchiveTensorEntry &e) {
    json += "{\"name\":\"" + json_escape(e.name) + "\"";
    json += ",\"slot_offset\":" + std::to_string(e.slot_offset);
    json += ",\"n_slots\":" + std::to_string(e.n_slots);
    json += ",\"n_floats\":" + std::to_string(e.n_floats);
    json += ",\"checksum\":\"" + e.checksum + "\"}";
}

} // namespace

int pack_safetensors_multi(const char *path, const char *output_crank, const char *manifest_path,
                           float alpha, float beta) {
    if (!path || !output_crank)
        return -1;

    std::vector<SafetensorsTensor> tensors;
    int rc = enumerate_safetensors_tensors(path, tensors);
    if (rc != 0)
        return rc;

    // Only F32 tensors are packable today; a file with none is an error, not an
    // empty archive, so callers never mistake silence for success.
    std::vector<SafetensorsTensor> packable;
    for (const auto &t : tensors)
        if (t.dtype == "F32")
            packable.push_back(t);
    if (packable.empty())
        return -5;
    if (packable.size() > CRANKL_MAX_TENSORS_PER_ARCHIVE)
        return -6;

    std::ifstream f(path, std::ios::binary);
    if (!f)
        return -2;

    std::vector<uint64_t> slots;
    std::vector<ArchiveTensorEntry> entries;
    std::vector<float> data;

    for (const auto &t : packable) {
        if (t.byte_len == 0 || t.byte_len > CRANKL_MAX_FLOAT_BYTES ||
            t.byte_len % sizeof(float) != 0)
            return -8;
        // byte_offset in the safetensors header is relative to the data section,
        // which starts after the 8-byte header length field.
        f.clear();
        f.seekg(static_cast<std::streamoff>(t.byte_offset));
        if (!f)
            return -7;
        data.resize(t.byte_len / sizeof(float));
        f.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(t.byte_len));
        if (!f)
            return -7;

        ArchiveTensorEntry e;
        e.name = t.name;
        e.n_floats = t.byte_len / sizeof(float);
        e.checksum = xxh64_hex(data.data(), t.byte_len);

        size_t before = slots.size();
        size_t need = pack::n_slots_from_count(e.n_floats);
        if (before + need > CRANKL_MAX_SLOTS)
            return -9;
        slots.resize(before + need);
        if (pack::fold_f32(data.data(), e.n_floats, slots.data() + before, need, alpha, beta) !=
            0)
            return -3;

        e.slot_offset = before;
        e.n_slots = need;
        entries.push_back(std::move(e));
    }

    // Footer JSON keeps model/hash first so legacy readers (which scan only the
    // first bytes of META) keep working on multi-tensor archives.
    std::string json = "{\"model\":\"multi\",\"hash\":\"";
    json += xxh64_hex(entries.data(), entries.size() * sizeof(ArchiveTensorEntry));
    json += "\",\"format_version\":2,\"tensor_count\":" + std::to_string(entries.size());
    json += ",\"tensors\":[";
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i)
            json += ",";
        append_tensor_json(json, entries[i]);
    }
    json += "]}";

    crankl_cran_header_t hdr{};
    hdr.n_slots = slots.size();
    hdr.depth_max = 1;
    hdr.gamma = 1.0f;
    hdr.flags = 1u; // metadata present

    std::vector<uint8_t> payload;
    payload.reserve(slots.size() * 8 + 8 + json.size());
    for (uint64_t w : slots)
        payload.insert(payload.end(), reinterpret_cast<uint8_t *>(&w),
                       reinterpret_cast<uint8_t *>(&w) + 8);
    uint32_t magic = FOOTER_MAGIC;
    uint32_t json_len = static_cast<uint32_t>(json.size());
    payload.insert(payload.end(), reinterpret_cast<uint8_t *>(&magic),
                   reinterpret_cast<uint8_t *>(&magic) + 4);
    payload.insert(payload.end(), reinterpret_cast<uint8_t *>(&json_len),
                   reinterpret_cast<uint8_t *>(&json_len) + 4);
    payload.insert(payload.end(), json.begin(), json.end());

    CranHeaderDisk hd{};
    std::memcpy(hd.magic, CRANK_MAGIC, 6);
    hd.version = 2;
    hd.n_slots = hdr.n_slots;
    hd.depth_max = hdr.depth_max;
    hd.gamma = hdr.gamma;
    hd.flags = hdr.flags;
    hd.checksum = crankl_xxhash64(payload.data(), payload.size(), 0);

    FILE *out = std::fopen(output_crank, "wb");
    if (!out)
        return -2;
    bool ok = std::fwrite(&hd, 1, sizeof(hd), out) == sizeof(hd) &&
              std::fwrite(payload.data(), 1, payload.size(), out) == payload.size();
    ok = std::fclose(out) == 0 && ok;
    if (!ok)
        return -2;

    if (manifest_path) {
        ArchiveMetrics m{};
        compute_archive_metrics(slots.data(), slots.size(), m);
        std::time_t now = std::time(nullptr);
        FILE *mf = std::fopen(manifest_path, "wb");
        if (!mf)
            return -4;
        std::fprintf(mf,
                     "{\n"
                     "  \"tool\": \"crankl\",\n"
                     "  \"version\": \"%s\",\n"
                     "  \"format_version\": 2,\n"
                     "  \"created_unix\": %lld,\n"
                     "  \"input\": \"%s\",\n"
                     "  \"output\": \"%s\",\n"
                     "  \"parent_run_id\": null,\n"
                     "  \"peels_applied\": [],\n"
                     "  \"finetune_loss_curve\": [],\n"
                     "  \"metrics\": {\n"
                     "    \"n_slots\": %llu,\n"
                     "    \"trit_density\": %.12g,\n"
                     "    \"trit_entropy\": %.12g,\n"
                     "    \"clifford_energy\": %.12g\n"
                     "  },\n"
                     "  \"tensors\": [",
                     CRANKL_VERSION_STRING, static_cast<long long>(now), path, output_crank,
                     static_cast<unsigned long long>(m.n_slots), m.trit_density, m.trit_entropy,
                     m.clifford_energy);
        for (size_t i = 0; i < entries.size(); ++i) {
            if (i)
                std::fprintf(mf, ",");
            std::fprintf(mf,
                         "\n    {\"name\":\"%s\",\"n_floats\":%llu,\"slot_offset\":%llu,"
                         "\"n_slots\":%llu,\"checksum\":\"%s\"}",
                         json_escape(entries[i].name).c_str(),
                         static_cast<unsigned long long>(entries[i].n_floats),
                         static_cast<unsigned long long>(entries[i].slot_offset),
                         static_cast<unsigned long long>(entries[i].n_slots),
                         entries[i].checksum.c_str());
        }
        std::fprintf(mf, "\n  ]\n}\n");
        if (std::fclose(mf) != 0)
            return -4;
    }
    return 0;
}

int read_archive_tensor_index(const ::crankl_cran_t *cran, std::vector<ArchiveTensorEntry> &out) {
    out.clear();
    if (!cran || !cran->mmap_base)
        return -1;
    if ((cran->header.flags & 1u) == 0)
        return 1; // no metadata footer at all

    const size_t payload_off = sizeof(CranHeaderDisk);
    const size_t slot_bytes = static_cast<size_t>(cran->header.n_slots) * 8;
    if (cran->mmap_size < payload_off + slot_bytes + 8)
        return -2;

    const auto *base = static_cast<const uint8_t *>(cran->mmap_base);
    const uint8_t *footer = base + payload_off + slot_bytes;
    uint32_t magic = 0;
    uint32_t json_len = 0;
    std::memcpy(&magic, footer, 4);
    std::memcpy(&json_len, footer + 4, 4);
    // The reader validates META json_len <= 4096; mirror that bound here.
    if (magic != FOOTER_MAGIC || json_len > 4096 ||
        cran->mmap_size < payload_off + slot_bytes + 8 + json_len)
        return -2;

    std::string json(reinterpret_cast<const char *>(footer + 8), json_len);
    size_t pos = json.find("\"tensors\":[");
    if (pos == std::string::npos)
        return 2; // metadata exists but carries no tensor index (classic archive)
    pos += 11;

    while (pos < json.size() && json[pos] != ']') {
        ArchiveTensorEntry e;
        size_t obj_end = json.find('}', pos);
        if (obj_end == std::string::npos)
            return -3;
        std::string block = json.substr(pos, obj_end - pos);

        size_t nq = block.find("\"name\":\"");
        if (nq == std::string::npos)
            return -3;
        nq += 8;
        size_t name_end = nq;
        while (name_end < block.size()) {
            if (block[name_end] == '\\') {
                name_end += 2;
                continue;
            }
            if (block[name_end] == '"')
                break;
            ++name_end;
        }
        e.name = block.substr(nq, name_end - nq);
        // Unescape the two escapes json_escape can produce.
        for (size_t k = e.name.find('\\'); k != std::string::npos && k + 1 < e.name.size();
             k = e.name.find('\\', k))
            e.name.erase(k, 1);

        auto grab_u64 = [&](const char *key, uint64_t &dst) {
            size_t p = block.find(key);
            if (p == std::string::npos)
                return false;
            dst = std::strtoull(block.c_str() + p + std::strlen(key), nullptr, 10);
            return true;
        };
        grab_u64("\"slot_offset\":", e.slot_offset);
        grab_u64("\"n_slots\":", e.n_slots);
        grab_u64("\"n_floats\":", e.n_floats);

        size_t cks = block.find("\"checksum\":\"");
        if (cks != std::string::npos) {
            cks += 12;
            size_t cke = block.find('"', cks);
            if (cke != std::string::npos)
                e.checksum = block.substr(cks, cke - cks);
        }

        out.push_back(std::move(e));
        pos = obj_end + 1;
        if (pos < json.size() && json[pos] == ',')
            ++pos;
    }
    return out.empty() ? 2 : 0;
}

} // namespace io
} // namespace crankl
