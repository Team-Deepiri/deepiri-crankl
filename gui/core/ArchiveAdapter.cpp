#include "core/ArchiveAdapter.h"

#include "crankl/cran.h"
#include "crankl/errors.h"
#include "crankl/metrics.h"

#include <QFileInfo>

#include <cstring>
#include <mutex>

namespace crankl_gui {

namespace {

// Serializes every open attempt process-wide -- crankl's reader keeps one
// global mapping, so two archives must never be open via the C API at once.
std::mutex &archiveApiMutex() {
    static std::mutex m;
    return m;
}

QString fixedCharsToString(const char *data, size_t capacity) {
    return QString::fromUtf8(data, static_cast<int>(strnlen(data, capacity)));
}

} // namespace

ArchiveOpenResult ArchiveAdapter::openArchive(const QString &path) {
    std::lock_guard<std::mutex> lock(archiveApiMutex());

    ArchiveOpenResult result;
    const QByteArray pathBytes = path.toUtf8();

    crankl_cran_t cran{};
    const int readRc = crankl_cran_read(pathBytes.constData(), &cran);
    if (readRc != CRANKL_OK) {
        result.ok = false;
        result.errorMessage = QString::fromUtf8(crankl_strerror(readRc));
        return result;
    }

    ArchiveSnapshot snapshot;
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

    result.ok = true;
    result.snapshot = std::move(snapshot);
    return result;
}

} // namespace crankl_gui
