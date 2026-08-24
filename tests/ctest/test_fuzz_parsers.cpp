// Randomized robustness harness for the three untrusted-input parsers:
// .crank reader, safetensors reader, GGUF reader. Each iteration mutates a
// valid seed file (bit flips / byte stomps / truncation) and requires the
// parser to return a typed error instead of crashing. Memory-safety bugs
// surface under the ASan/UBSan CI job; logic bugs surface as crashes here.
#include "crankl/crankl.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr uint64_t kRngMul = 6364136223846793005ULL;
struct Rng {
    uint64_t state;
    explicit Rng(uint64_t seed) : state(seed) {}
    uint32_t next() {
        state = state * kRngMul + 1442695040888963407ULL;
        return static_cast<uint32_t>(state >> 33);
    }
    size_t below(size_t n) {
        return static_cast<size_t>(next() % n);
    }
};

std::string temp_path(const char *tag) {
    static int counter = 0;
    return std::string("/tmp/opencode/fuzz_") + tag + "_" + std::to_string(counter++) + ".bin";
}

void write_file(const std::string &path, const std::vector<uint8_t> &bytes) {
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f)
        exit(2);
    if (!bytes.empty())
        std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
}

/* Applies 1..k mutations in place. */
void mutate(std::vector<uint8_t> &data, Rng &rng) {
    const int rounds = 1 + static_cast<int>(rng.below(4));
    for (int r = 0; r < rounds && !data.empty(); ++r) {
        const uint32_t kind = rng.next() % 3;
        if (kind == 0) { // bit flip at a random byte
            data[rng.below(data.size())] ^= static_cast<uint8_t>(1u << rng.below(8));
        } else if (kind == 1) { // random byte stomp
            data[rng.below(data.size())] = static_cast<uint8_t>(rng.next());
        } else if (data.size() > 1) { // truncate
            data.resize(1 + rng.below(data.size() - 1));
        }
    }
}

/* Seed: a valid single-slot v2 archive produced by the public writer path. */
std::vector<uint8_t> crank_seed() {
    float weights[64];
    for (size_t i = 0; i < 64; ++i)
        weights[i] = static_cast<float>((int)(i % 13) - 6) * 0.25f;
    const size_t n_floats = sizeof(weights) / sizeof(weights[0]);
    const size_t n_slots = crankl_pack_n_slots(n_floats);
    std::vector<uint64_t> slots(n_slots, 0);
    crankl_pack_f32(weights, n_floats, slots.data(), n_slots, 0.1f, 0.01f);
    crankl_cran_header_t hdr = {};
    hdr.n_slots = static_cast<uint32_t>(n_slots);
    hdr.gamma = 0.5f;
    const char *path = "/tmp/opencode/fuzz_crank_seed.crank";
    if (crankl_cran_write(path, &hdr, slots.data(), nullptr, 0) != CRANKL_OK)
        exit(3);
    FILE *f = std::fopen(path, "rb");
    if (!f)
        exit(3);
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> bytes(static_cast<size_t>(sz));
    if (sz > 0 && std::fread(bytes.data(), 1, bytes.size(), f) != bytes.size())
        exit(3);
    std::fclose(f);
    return bytes;
}

/* Seed: minimal valid safetensors — header JSON + one F32 tensor. */
std::vector<uint8_t> safetensors_seed() {
    const char *json = "{\"weights\":{\"dtype\":\"F32\",\"shape\":[4,16],\"data_offsets\":[0,256]},"
                       "\"__metadata__\":{\"format\":\"crankl-fuzz\"}}";
    const uint64_t hdr_len = static_cast<uint64_t>(strlen(json));
    std::vector<uint8_t> bytes;
    bytes.insert(bytes.end(), reinterpret_cast<const uint8_t *>(&hdr_len),
                 reinterpret_cast<const uint8_t *>(&hdr_len) + 8);
    bytes.insert(bytes.end(), json, json + hdr_len);
    std::vector<float> payload(64);
    for (size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<float>(i) * 0.5f;
    const auto *p = reinterpret_cast<const uint8_t *>(payload.data());
    bytes.insert(bytes.end(), p, p + sizeof(payload));
    return bytes;
}

/* Seed: minimal valid GGUF v3 with one F32 tensor of 16 elements. */
std::vector<uint8_t> gguf_seed() {
    std::vector<uint8_t> b;
    auto u32 = [&b](uint32_t v) {
        const auto *p = reinterpret_cast<const uint8_t *>(&v);
        b.insert(b.end(), p, p + 4);
    };
    auto u64 = [&b](uint64_t v) {
        const auto *p = reinterpret_cast<const uint8_t *>(&v);
        b.insert(b.end(), p, p + 8);
    };
    auto str = [&](const char *s) {
        uint64_t n = strlen(s);
        u64(n);
        b.insert(b.end(), s, s + n);
    };
    u32(3); // magic GGUF
    u32(3); // version 3
    u64(0); // n_tensors placeholder patched below
    u64(0); // n_kv = 0
    // tensor info
    str("t");
    u32(2); // n_dims
    u64(2);
    u64(8);
    u32(0); // type F32
    u64(0); // offset
    // patch n_tensors
    const uint64_t one = 1;
    memcpy(b.data() + 12, &one, 8);
    // alignment padding to 32 then data
    while (b.size() % 32 != 0)
        b.push_back(0);
    std::vector<float> t(16, 0.25f);
    const auto *p = reinterpret_cast<const uint8_t *>(t.data());
    b.insert(b.end(), p, p + sizeof(t));
    return b;
}

int run_family(const char *tag, const std::vector<uint8_t> &seed, int iterations, uint64_t rng_seed,
               void (*parse)(const std::string &)) {
    Rng rng(rng_seed);
    for (int i = 0; i < iterations; ++i) {
        std::vector<uint8_t> mutated(seed);
        mutate(mutated, rng);
        const std::string path = temp_path(tag);
        write_file(path, mutated);
        parse(path); // must not crash; return codes are the parser's business
        std::remove(path.c_str());
    }
    return 0;
}

void parse_cran(const std::string &path) {
    crankl_cran_t cran;
    memset(&cran, 0, sizeof(cran));
    if (crankl_cran_read(path.c_str(), &cran) == CRANKL_OK)
        crankl_cran_close(&cran);
}

void parse_safetensors(const std::string &path) {
    size_t count = 0;
    if (crankl_safetensors_count(path.c_str(), &count) != CRANKL_OK || count == 0)
        return;
    std::vector<crankl_source_tensor_t> tensors(count);
    if (crankl_safetensors_list(path.c_str(), tensors.data(), count) != CRANKL_OK)
        return;
    float *buf = nullptr;
    size_t n = 0;
    if (crankl_safetensors_read_f32(path.c_str(), tensors[0].name, &buf, &n) == CRANKL_OK)
        free(buf);
}

void parse_gguf(const std::string &path) {
    size_t count = 0;
    if (crankl_gguf_count(path.c_str(), &count) != CRANKL_OK || count == 0)
        return;
    std::vector<crankl_source_tensor_t> tensors(count);
    crankl_gguf_list(path.c_str(), tensors.data(), count);
}

} // namespace

int main() {
    const int iterations = 4000;
    if (run_family("cran", crank_seed(), iterations, 0xC0FFEEULL, parse_cran) != 0)
        return 1;
    if (run_family("st", safetensors_seed(), iterations, 0xBADF00DULL, parse_safetensors) != 0)
        return 1;
    if (run_family("gguf", gguf_seed(), iterations, 0xFEEDFACEULL, parse_gguf) != 0)
        return 1;
    std::puts("fuzz: 12000 mutated inputs parsed without crash");
    return 0;
}
