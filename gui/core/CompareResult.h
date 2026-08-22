#ifndef CRANKL_GUI_CORE_COMPARE_RESULT_H
#define CRANKL_GUI_CORE_COMPARE_RESULT_H

#include "core/ArchiveSnapshot.h"

#include <QString>

#include <cstdint>
#include <vector>

namespace crankl_gui {

// Result of comparing two .crank archives via the C API (diff.h, sheaf.h,
// clifford.h, metrics.h) on slots copied out of both mappings. All deltas are
// (B - A). A single bad read fails the whole comparison with ok=false and an
// error string naming the failing archive -- a comparison must never silently
// compare against an empty side.
struct CompareResult {
    bool ok = false;
    QString errorMessage;
    QString pathA;
    QString pathB;

    uint64_t slotsA = 0;
    uint64_t slotsB = 0;
    uint64_t slotsCompared = 0; // min(a, b) -- mismatched tails are reported, never hidden
    uint64_t slotsChanged = 0;
    double hamming = 0.0;           // normalized bit Hamming over compared slots
    double cliffordResonance = 0.0; // mean per-slot Clifford resonance
    double sheafResonance = 0.0;

    // Per-archive metrics (B - A deltas are derived by the UI).
    ArchiveMetrics metricsA;
    ArchiveMetrics metricsB;

    // The first `changedSlots` differences, each with the two raw words so the
    // UI can show real hex without re-opening the archives. Bounded: a million
    // changed slots must not balloon the result into megabytes.
    static constexpr uint32_t kMaxChangedSlotsShown = 200;
    struct ChangedSlot {
        uint32_t index;
        uint64_t wordA;
        uint64_t wordB;
    };
    std::vector<ChangedSlot> changedSlots;
};

} // namespace crankl_gui

Q_DECLARE_METATYPE(crankl_gui::CompareResult)

#endif // CRANKL_GUI_CORE_COMPARE_RESULT_H