// Thread-safety contract test: the library holds no mutable global state, so
// independent handles must be usable concurrently from multiple threads.
// Proves it empirically: N threads read the same archive through their own
// handles and run identical holonomy batches; every result must be bitwise
// equal to the single-threaded reference.
#include "crankl/crankl.h"

#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

namespace {

constexpr int kThreads = 8;
constexpr size_t kBatchesPerThread = 20;
constexpr size_t kBatch = 64;
constexpr size_t kDim = 64;

bool same_bytes(const float *a, const float *b, size_t n) {
    return std::memcmp(a, b, n * sizeof(float)) == 0;
}

} // namespace

int main() {
    // Build a deterministic archive with enough blocks to matter.
    const size_t n_floats = 512 * 64;
    std::vector<float> weights(n_floats);
    for (size_t i = 0; i < n_floats; ++i)
        weights[i] = static_cast<float>((int)(i % 251) - 125) / 32.0f;
    const size_t n_slots = crankl_pack_n_slots(n_floats);
    std::vector<uint64_t> slots(n_slots, 0);
    if (crankl_pack_f32(weights.data(), n_floats, slots.data(), n_slots, 0.5f, 0.05f) !=
        CRANKL_OK) {
        std::puts("FAIL: seed pack");
        return 2;
    }
    crankl_cran_header_t hdr = {};
    hdr.n_slots = static_cast<uint32_t>(n_slots);
    hdr.gamma = 0.5f;
    const char *path = "threading_contract.crank";
    if (crankl_cran_write(path, &hdr, slots.data(), nullptr, 0) != CRANKL_OK) {
        std::puts("FAIL: archive write");
        return 2;
    }

    // Single-threaded reference output.
    std::vector<float> x(kBatch * kDim);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = static_cast<float>((int)(i % 97) - 48) / 16.0f;
    std::vector<float> ref(kBatch * kDim);

    {
        crankl_cran_t cran{};
        if (crankl_cran_read(path, &cran) != CRANKL_OK) {
            std::puts("FAIL: reference read");
            return 2;
        }
        if (crankl_holonomy_batch(&cran, x.data(), kDim, kBatch, ref.data()) != CRANKL_OK) {
            std::puts("FAIL: reference forward");
            return 2;
        }
        crankl_cran_close(&cran);
    }

    // Concurrent workers: own handle per thread, repeated open/close cycles
    // interleaved with batched forwards.
    std::vector<int> failures(kThreads, 0);
    std::vector<std::thread> pool;
    for (int t = 0; t < kThreads; ++t)
        pool.emplace_back([t, path, &x, &ref, &failures]() {
            std::vector<float> y(kBatch * kDim);
            for (size_t r = 0; r < kBatchesPerThread; ++r) {
                crankl_cran_t cran{};
                if (crankl_cran_read(path, &cran) != CRANKL_OK) {
                    ++failures[t];
                    continue;
                }
                if (crankl_holonomy_batch(&cran, x.data(), kDim, kBatch, y.data()) !=
                        CRANKL_OK ||
                    !same_bytes(y.data(), ref.data(), y.size()))
                    ++failures[t];
                crankl_cran_close(&cran);
            }
        });
    for (auto &th : pool)
        th.join();

    int total = 0;
    for (int f : failures)
        total += f;
    std::remove(path);
    if (total) {
        std::printf("FAIL: %d concurrent mismatches across %d threads\n", total, kThreads);
        return 1;
    }
    std::printf(
        "threading ok: %d threads x %zu batches, bitwise-identical to serial\n", kThreads,
        kBatchesPerThread);
    return 0;
}
