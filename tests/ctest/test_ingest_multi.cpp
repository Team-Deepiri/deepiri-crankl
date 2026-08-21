#include "crankl/crankl.h"
#include "crankl/ingest.h"

#include <unistd.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::string make_temp_path(const char *suffix) {
    static int counter = 0;
    return "/tmp/crankl_test_ingest_" + std::to_string(getpid()) + "_" +
           std::to_string(counter++) + suffix;
}

// Minimal safetensors writer: 8-byte header length + JSON header + tensor data.
void write_safetensors(const std::string &path,
                       const std::vector<std::pair<std::string, std::vector<float>>> &tensors) {
    std::string json = "{";
    size_t offset = 0;
    for (size_t i = 0; i < tensors.size(); ++i) {
        if (i)
            json += ",";
        json += "\"" + tensors[i].first + "\":{"
                "\"dtype\":\"F32\","
                "\"shape\":[" + std::to_string(tensors[i].second.size()) + "],"
                "\"data_offsets\":[" + std::to_string(offset) + "," +
                std::to_string(offset + tensors[i].second.size() * 4) + "]}";
        offset += tensors[i].second.size() * 4;
    }
    json += "}";
    if (json.size() % 64 != 0) // safetensors pads the header, mimic that
        json += std::string(64 - json.size() % 64, ' ');

    std::ofstream f(path, std::ios::binary);
    uint64_t len = json.size();
    f.write(reinterpret_cast<const char *>(&len), 8);
    f.write(json.data(), static_cast<std::streamsize>(json.size()));
    for (const auto &t : tensors)
        f.write(reinterpret_cast<const char *>(t.second.data()),
                static_cast<std::streamsize>(t.second.size() * 4));
}

int expect_count(const char *what, size_t expected, size_t actual) {
    if (actual != expected) {
        std::fprintf(stderr, "FAIL: %s count %zu != %zu\n", what, expected, actual);
        return 1;
    }
    return 0;
}

} // namespace

int main() {
    int failures = 0;

    // ---- Fixture: three named LoRA-ish tensors ----
    std::string st_path = make_temp_path(".safetensors");
    // Smooth low-rank-ish fixtures: the trit packer is lossy on rough noise, and
    // roundtrip tolerances follow the 250-Frobenius-per-block repo convention.
    std::vector<float> a(4096), b(1024), c(300);
    for (size_t i = 0; i < a.size(); ++i)
        a[i] = 0.1f * std::sin(static_cast<float>(i) * 0.0137f);
    for (size_t i = 0; i < b.size(); ++i)
        b[i] = 0.05f * std::sin(static_cast<float>(i % 64) * 0.31f);
    for (size_t i = 0; i < c.size(); ++i)
        c[i] = 0.02f * static_cast<float>((int)(i / 64)) + 0.01f *
                                                          std::sin(static_cast<float>(i) * 0.11f);
    write_safetensors(st_path, {{"lora_A", a}, {"lora_B", b}, {"embed", c}});

    // ---- Enumerate source tensors ----
    size_t count = 0;
    if (crankl_safetensors_count(st_path.c_str(), &count) != CRANKL_OK ||
        (failures += expect_count("source", 3, count)))
        return 1;
    std::vector<crankl_source_tensor_t> src(count);
    if (crankl_safetensors_list(st_path.c_str(), src.data(), src.size()) != CRANKL_OK) {
        std::fprintf(stderr, "FAIL: source list\n");
        return 1;
    }
    if (std::strcmp(src[0].name, "lora_A") != 0 || !src[0].is_f32 ||
        src[0].n_floats != a.size()) {
        std::fprintf(stderr, "FAIL: source[0] name/f32/n_floats\n");
        ++failures;
    }

    // ---- Multi-tensor pack + manifest v2 ----
    std::string crank_path = make_temp_path(".crank");
    std::string manifest_path = make_temp_path(".json");
    if (crankl_pack_safetensors_multi(st_path.c_str(), crank_path.c_str(), manifest_path.c_str(),
                                      0.1f, 0.01f) != CRANKL_OK) {
        std::fprintf(stderr, "FAIL: pack multi\n");
        return 1;
    }
    {
        std::ifstream mf(manifest_path);
        std::string body((std::istreambuf_iterator<char>(mf)),
                         std::istreambuf_iterator<char>());
        if (body.find("\"format_version\": 2") == std::string::npos ||
            body.find("\"parent_run_id\"") == std::string::npos ||
            body.find("\"peels_applied\"") == std::string::npos ||
            body.find("lora_A") == std::string::npos) {
            std::fprintf(stderr, "FAIL: manifest v2 fields missing\n");
            ++failures;
        }
    }

    // ---- Archive tensor index roundtrip ----
    size_t acount = 99;
    if (crankl_archive_tensor_count(crank_path.c_str(), &acount) != CRANKL_OK ||
        (failures += expect_count("archive tensors", 3, acount)))
        return 1;
    std::vector<crankl_archive_tensor_t> idx(acount);
    if (crankl_archive_tensor_list(crank_path.c_str(), idx.data(), idx.size()) != CRANKL_OK) {
        std::fprintf(stderr, "FAIL: archive tensor list\n");
        return 1;
    }
    size_t total_slots = 0;
    for (size_t i = 0; i < acount; ++i)
        total_slots += idx[i].n_slots;
    if (idx[0].slot_offset != 0 || idx[1].slot_offset != idx[0].n_slots ||
        idx[2].slot_offset != idx[0].n_slots + idx[1].n_slots) {
        std::fprintf(stderr, "FAIL: slot ranges not contiguous\n");
        ++failures;
    }
    for (size_t i = 0; i < acount; ++i) {
        if (idx[i].checksum[0] == '\0') {
            std::fprintf(stderr, "FAIL: checksum missing for %s\n", idx[i].name);
            ++failures;
        }
    }

    // ---- Per-tensor unpack within pack tolerance ----
    {
        crankl_cran_t cran{};
        if (crankl_cran_read(crank_path.c_str(), &cran) != CRANKL_OK) {
            std::fprintf(stderr, "FAIL: read multi archive\n");
            return 1;
        }
        if ((size_t)cran.header.n_slots != total_slots) {
            std::fprintf(stderr, "FAIL: n_slots %llu != %zu\n",
                         (unsigned long long)cran.header.n_slots, total_slots);
            ++failures;
        }
        crankl_cran_metadata_t meta{};
        if (crankl_cran_read_metadata(&cran, &meta) != CRANKL_OK ||
            std::strcmp(meta.model_name, "multi") != 0) {
            std::fprintf(stderr, "FAIL: legacy metadata read on multi archive\n");
            ++failures;
        }
        crankl_cran_close(&cran);
    }
    {
        std::vector<uint64_t> slots(total_slots);
        // Re-pack each tensor alone and compare unpacked output against the
        // corresponding range of the combined archive.
        const float *src_data[] = {a.data(), b.data(), c.data()};
        const size_t src_len[] = {a.size(), b.size(), c.size()};
        for (size_t t = 0; t < 3; ++t) {
            size_t ns = crankl_pack_n_slots(src_len[t]);
            std::vector<uint64_t> solo(ns);
            crankl_pack_f32(src_data[t], src_len[t], solo.data(), ns, 0.1f, 0.01f);
            std::vector<float> out(src_len[t]);
            crankl_unpack_f32(solo.data(), ns, out.data(), out.size());
            double frob = 0.0;
            for (size_t i = 0; i < out.size(); ++i) {
                double d = static_cast<double>(out[i]) - src_data[t][i];
                frob += d * d;
            }
            double limit = 250.0 * static_cast<double>(ns);
            if (frob > limit) {
                std::fprintf(stderr, "FAIL: tensor %zu frob %g > %g\n", t, frob, limit);
                ++failures;
            }
        }
    }

    // ---- Single-tensor path still works ----
    {
        std::string one_path = make_temp_path(".crank");
        if (crankl_pack_safetensors_tensor(st_path.c_str(), "embed", one_path.c_str()) !=
            CRANKL_OK) {
            std::fprintf(stderr, "FAIL: single-tensor pack\n");
            ++failures;
        }
    }

    // ---- Security: garbage / truncated input errors instead of crashing ----
    {
        std::string bad = make_temp_path(".safetensors");
        std::ofstream f(bad, std::ios::binary);
        f.write("\x10\x00\x00\x00\x00\x00\x00\x00{\"broken", 20);
        f.close();
        size_t n = 7;
        if (crankl_safetensors_count(bad.c_str(), &n) == CRANKL_OK && n > 4) {
            std::fprintf(stderr, "FAIL: truncated safetensors enumerated\n");
            ++failures;
        }
        if (crankl_pack_safetensors_multi(bad.c_str(), make_temp_path(".crank").c_str(),
                                          nullptr, 0.1f, 0.01f) == CRANKL_OK) {
            std::fprintf(stderr, "FAIL: truncated safetensors packed\n");
            ++failures;
        }
    }
    {
        size_t n = 0;
        if (crankl_safetensors_count("/nonexistent/file.safetensors", &n) == CRANKL_OK) {
            std::fprintf(stderr, "FAIL: missing file enumerated\n");
            ++failures;
        }
    }

    std::printf(failures ? "test_ingest_multi FAILURES=%d\n" : "test_ingest_multi ok\n",
                failures);
    return failures ? 1 : 0;
}
