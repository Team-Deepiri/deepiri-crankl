#include "pack/persistence.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace crankl {
namespace pack {

std::vector<PersistencePair> persistence_diagram_1d(const float *data, size_t n) {
    std::vector<PersistencePair> diagram;
    if (n < 2)
        return diagram;

    struct Event {
        float value;
        size_t index;
        bool is_max;
    };
    std::vector<Event> events;
    events.reserve(n);
    for (size_t i = 0; i < n; ++i)
        events.push_back({data[i], i, true});

    std::sort(events.begin(), events.end(),
              [](const Event &a, const Event &b) { return a.value < b.value; });

    std::vector<int> parent(n);
    for (size_t i = 0; i < n; ++i)
        parent[i] = static_cast<int>(i);

    auto find = [&](int x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };

    auto unite = [&](int a, int b, float death) {
        int ra = find(a), rb = find(b);
        if (ra == rb)
            return;
        float birth = events.empty() ? 0.0f : events.back().value;
        diagram.push_back({birth, death});
        parent[rb] = ra;
    };

    for (size_t i = 0; i < n; ++i) {
        if (i > 0)
            unite(static_cast<int>(i), static_cast<int>(i - 1), data[i]);
        if (i + 1 < n)
            unite(static_cast<int>(i), static_cast<int>(i + 1), data[i]);
    }

    if (diagram.empty()) {
        float minv = *std::min_element(data, data + n);
        float maxv = *std::max_element(data, data + n);
        if (maxv > minv)
            diagram.push_back({minv, maxv});
    }
    return diagram;
}

float wasserstein_persistence(const std::vector<PersistencePair> &a,
                              const std::vector<PersistencePair> &b) {
    if (a.empty() && b.empty())
        return 0.0f;
    size_t n = std::max(a.size(), b.size());
    float sum = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        float ab = i < a.size() ? a[i].birth : 0.0f;
        float ad = i < a.size() ? a[i].death : 0.0f;
        float bb = i < b.size() ? b[i].birth : 0.0f;
        float bd = i < b.size() ? b[i].death : 0.0f;
        sum += std::fabs(ab - bb) + std::fabs(ad - bd);
    }
    return sum / static_cast<float>(n);
}

float spectral_range(const float *data, size_t n) {
    if (n == 0)
        return 0.0f;
    float minv = data[0], maxv = data[0];
    for (size_t i = 1; i < n; ++i) {
        minv = std::min(minv, data[i]);
        maxv = std::max(maxv, data[i]);
    }
    return maxv - minv;
}

} // namespace pack
} // namespace crankl
