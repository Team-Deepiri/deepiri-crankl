#ifndef CRANKL_GUI_CORE_ARCHIVE_SNAPSHOT_H
#define CRANKL_GUI_CORE_ARCHIVE_SNAPSHOT_H

#include <QDateTime>
#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

namespace crankl_gui {

// Only three values ever exist -- pass, fail, not yet verified. The rail
// colour on ArchiveHealthHeader is driven directly by this enum.
enum class VerifyState { Pass, Fail, Unverified };

struct ArchiveMetadata {
    QString modelName;
    QString sourceHash;
};

struct ArchiveMetrics {
    uint64_t nSlots = 0;
    uint32_t depthMin = 0;
    uint32_t depthMax = 0;
    double scalarMean = 0.0;
    double scalarAbsMean = 0.0;
    double tritDensity = 0.0;
    double tritEntropy = 0.0;
    double cliffordEnergy = 0.0;
    double beta1Proxy = 0.0; // never rendered without the word "proxy" attached
};

// Fully owned, plain-data snapshot of a .crank archive.
struct ArchiveSnapshot {
    QString path;
    QString fileName;
    qint64 byteSize = 0;

    VerifyState verifyState = VerifyState::Unverified;
    QDateTime verifiedAt;

    // Populated only when verifyState == Pass -- a failing archive shows no
    // metrics and no slot browser, per the design's verify-fail state.
    ArchiveMetrics metrics;
    std::optional<ArchiveMetadata> metadata;

    // Always false in Phase 1: no code path here validates or trusts a real
    // history stack (see docs/CRANK_FORMAT.md's pointer-inequality bug).
    // HomePage and InspectPage must always render "Rollback unavailable"
    // while this is false, and Phase 1 never sets it to true.
    bool historyAvailable = false;

    float gamma = 0.0f;
    uint32_t flags = 0;

    std::vector<uint64_t> crankWords;

    bool isEmpty() const { return path.isEmpty(); }
};

} // namespace crankl_gui

#endif // CRANKL_GUI_CORE_ARCHIVE_SNAPSHOT_H
