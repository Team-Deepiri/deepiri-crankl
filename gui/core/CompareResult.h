#ifndef CRANKL_GUI_CORE_COMPARE_RESULT_H
#define CRANKL_GUI_CORE_COMPARE_RESULT_H

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

    double deltaDepthMin = 0.0;
    double deltaDepthMax = 0.0;
    double deltaScalarMean = 0.0;
    double deltaScalarAbsMean = 0.0;
    double deltaTritDensity = 0.0;
    double deltaTritEntropy = 0.0;
    double deltaEnergy = 0.0;
    double deltaBeta1Proxy = 0.0;

    // Indices (into the compared prefix) where the two words differ, in
    // ascending order. Kept so the Compare page can render a per-slot list.
    std::vector<uint32_t> changedIndices;
};

} // namespace crankl_gui

Q_DECLARE_METATYPE(crankl_gui::CompareResult)

#endif // CRANKL_GUI_CORE_COMPARE_RESULT_H