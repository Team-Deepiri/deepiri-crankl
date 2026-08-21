#include "core/ArchiveAdapter.h"

#include "crankl/cran.h"
#include "crankl/errors.h"
#include "crankl/metrics.h"
#include "crankl/clifford.h"
#include "crankl/diff.h"
#include "crankl/sheaf.h"

#include <QFileInfo>

#include <algorithm>
#include <cstring>
#include <mutex>

namespace crankl_gui {

namespace {

// Serializes every open attempt process-wide. The reader's per-handle mmaps
// are independent today (any number of handles may be open at once), but the
// C API's headers still document a single-global-mapping history, so keeping
// reads serialized is the conservative, correct-by-construction choice.
std::mutex &archiveApiMutex() {
    static std::mutex m;
    return m;
}

QString fixedCharsToString(const char *data, size_t capacity) {
    return QString::fromUtf8(data, static_cast<int>(strnlen(data, capacity)));
}

// Reads one archive into a snapshot; returns CRANKL_OK or the read/verify
// failure code. On success the caller owns `snapshot` and `cran`'s mapping
// (closed by the caller). On failure only `cran` may hold a partial mapping.
int loadSnapshot(const QString &path, ArchiveSnapshot &snapshot) {
    const QByteArray pathBytes = path.toUtf8();
    const char *cPath = pathBytes.constData();

    crankl_cran_t cran{};
    const int readRc = crankl_cran_read(cPath, &cran);
    if (readRc != CRANKL_OK)
        return readRc;

    snapshot.path = path;
    snapshot.fileName = QFileInfo(path).fileName();
    snapshot.byteSize = static_cast<qint64>(cran.mmap_size);
    snapshot.gamma = cran.header.gamma;
    snapshot.flags = cran.header.flags;

    // Fixed §2.3 sentence: no code path in this phase validates a real
    // history stack, so this is always false regardless of cran.layers --
    // that pointer is documented as unreliable for presence-detection.
    snapshot.historyAvailable = false;

    const int verifyRc = crankl_cran_verify(&cran);
    snapshot.verifyState = (verifyRc == CRANKL_OK) ? VerifyState::Pass : VerifyState::Fail;
    snapshot.verifyError =
        (verifyRc == CRANKL_OK) ? QString() : QString::fromUtf8(crankl_strerror(verifyRc));
    snapshot.verifiedAt = QDateTime::currentDateTime();

    if (snapshot.verifyState == VerifyState::Pass) {
        crankl_archive_metrics_t metrics{};
        if (crankl_cran_compute_metrics(&cran, &metrics) == CRANKL_OK) {
            snapshot.metrics.nSlots = metrics.n_slots;
            snapshot.metrics.depthMin = metrics.depth_min;
            snapshot.metrics.depthMax = metrics.depth_max;
            snapshot.metrics.scalarMean = metrics.scalar_mean;
            snapshot.metrics.scalarAbsMean = metrics.scalar_abs_mean;
            snapshot.metrics.tritDensity = metrics.trit_density;
            snapshot.metrics.tritEntropy = metrics.trit_entropy;
            snapshot.metrics.cliffordEnergy = metrics.clifford_energy;
            snapshot.metrics.beta1Proxy = metrics.beta1_proxy;
            snapshot.metricsValid = true;
        }
        // On failure metricsValid stays false and `metrics` stays zeroed. The
        // UI must render "unavailable" from that flag rather than nine zeros,
        // which would be indistinguishable from a real all-zero archive.

        crankl_cran_metadata_t meta{};
        const int metaRc = crankl_cran_read_metadata(&cran, &meta);
        if (metaRc == CRANKL_OK) {
            ArchiveMetadata m;
            m.modelName = fixedCharsToString(meta.model_name, sizeof(meta.model_name));
            m.sourceHash = fixedCharsToString(meta.source_hash, sizeof(meta.source_hash));
            snapshot.metadata = m;
            snapshot.metadataState = MetadataState::Present;
        } else if (metaRc == CRANKL_ERR_NO_METADATA) {
            // No provenance footer. Normal and expected -- rendered as
            // "none"/"no metadata", never as an error.
            snapshot.metadataState = MetadataState::Absent;
        } else {
            // NULL/INVALID/IO/FORMAT: the footer could not be read,
            // surfacing it as "none" would report a damaged
            // archive as a clean one.
            snapshot.metadataState = MetadataState::Error;
            snapshot.metadataError = QString::fromUtf8(crankl_strerror(metaRc));
        }

        if (cran.slots && cran.header.n_slots > 0) {
            snapshot.crankWords.assign(cran.slots, cran.slots + cran.header.n_slots);
        }
    }
    // Metrics and slots stay empty for a failing archive -- the verify-fail
    // card explicitly keeps them hidden rather than showing partial data.

    crankl_cran_close(&cran);
    return CRANKL_OK;
}

} // namespace

ArchiveOpenResult ArchiveAdapter::openArchive(const QString &path) {
    std::lock_guard<std::mutex> lock(archiveApiMutex());

    ArchiveOpenResult result;
    ArchiveSnapshot snapshot;
    const int rc = loadSnapshot(path, snapshot);
    if (rc != CRANKL_OK) {
        result.ok = false;
        result.errorMessage =
            QObject::tr("Could not open %1: %2").arg(path, QString::fromUtf8(crankl_strerror(rc)));
        return result;
    }

    result.ok = true;
    result.snapshot = std::move(snapshot);
    return result;
}

CompareResult ArchiveAdapter::compareArchives(const QString &pathA, const QString &pathB) {
    std::lock_guard<std::mutex> lock(archiveApiMutex());

    CompareResult result;
    result.pathA = pathA;
    result.pathB = pathB;

    ArchiveSnapshot snapA;
    const int rcA = loadSnapshot(pathA, snapA);
    if (rcA != CRANKL_OK) {
        result.errorMessage = QObject::tr("Could not open %1: %2")
                                  .arg(pathA, QString::fromUtf8(crankl_strerror(rcA)));
        return result;
    }
    ArchiveSnapshot snapB;
    const int rcB = loadSnapshot(pathB, snapB);
    if (rcB != CRANKL_OK) {
        result.errorMessage = QObject::tr("Could not open %1: %2")
                                  .arg(pathB, QString::fromUtf8(crankl_strerror(rcB)));
        return result;
    }

    const std::vector<uint64_t> &slotsA = snapA.crankWords;
    const std::vector<uint64_t> &slotsB = snapB.crankWords;
    result.slotsA = slotsA.size();
    result.slotsB = slotsB.size();

    const size_t n = std::min(slotsA.size(), slotsB.size());
    result.slotsCompared = n;
    result.slotsChanged = crankl_crank_diff_count(slotsA.data(), slotsB.data(), n);
    result.hamming = crankl_crank_diff_hamming(slotsA.data(), slotsB.data(), n);
    result.sheafResonance = crankl_sheaf_resonance(slotsA.data(), slotsA.size(), slotsB.data(),
                                                   slotsB.size());

    double cliffordSum = 0.0;
    for (size_t i = 0; i < n; ++i)
        cliffordSum += crankl_clifford_resonance(slotsA[i], slotsB[i]);
    result.cliffordResonance = n > 0 ? cliffordSum / static_cast<double>(n) : 0.0;

    result.changedSlots.reserve(
        std::min<size_t>(CompareResult::kMaxChangedSlotsShown, n));
    for (size_t i = 0; i < n; ++i) {
        if (slotsA[i] != slotsB[i]) {
            if (result.changedSlots.size() < CompareResult::kMaxChangedSlotsShown) {
                result.changedSlots.push_back(
                    {static_cast<uint32_t>(i), slotsA[i], slotsB[i]});
            } else {
                break; // total count already known from crankl_crank_diff_count
            }
        }
    }

    crankl_archive_metrics_t ma{}, mb{};
    crankl_compute_archive_metrics(slotsA.data(), slotsA.size(), &ma);
    crankl_compute_archive_metrics(slotsB.data(), slotsB.size(), &mb);
    auto copyMetrics = [](const crankl_archive_metrics_t &src, ArchiveMetrics &dst) {
        dst.nSlots = src.n_slots;
        dst.depthMin = src.depth_min;
        dst.depthMax = src.depth_max;
        dst.scalarMean = src.scalar_mean;
        dst.scalarAbsMean = src.scalar_abs_mean;
        dst.tritDensity = src.trit_density;
        dst.tritEntropy = src.trit_entropy;
        dst.cliffordEnergy = src.clifford_energy;
        dst.beta1Proxy = src.beta1_proxy;
    };
    copyMetrics(ma, result.metricsA);
    copyMetrics(mb, result.metricsB);

    result.ok = true;
    return result;
}

} // namespace crankl_gui