#include "widgets/ResultSummaryPanel.h"

#include "widgets/MetricsPanel.h"
#include "widgets/StyleUtil.h"

#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace crankl_gui {

QJsonObject ResultSummaryPanel::snapshotToJson(const ArchiveSnapshot &snapshot) {
    QJsonObject obj;
    obj.insert(QStringLiteral("path"), snapshot.path);
    obj.insert(QStringLiteral("file_name"), snapshot.fileName);
    obj.insert(QStringLiteral("byte_size"), snapshot.byteSize);
    obj.insert(QStringLiteral("gamma"), snapshot.gamma);
    obj.insert(QStringLiteral("flags"), static_cast<qint64>(snapshot.flags));
    switch (snapshot.metadataState) {
    case MetadataState::Present:
        obj.insert(QStringLiteral("has_metadata"), true);
        break;
    case MetadataState::Absent:
        obj.insert(QStringLiteral("has_metadata"), false);
        break;
    case MetadataState::Error:
        obj.insert(QStringLiteral("has_metadata"), QStringLiteral("error"));
        break;
    }
    if (snapshot.metricsValid) {
        QJsonObject metrics;
        const ArchiveMetrics &m = snapshot.metrics;
        metrics.insert(QStringLiteral("n_slots"), static_cast<qint64>(m.nSlots));
        metrics.insert(QStringLiteral("depth_min"), static_cast<qint64>(m.depthMin));
        metrics.insert(QStringLiteral("depth_max"), static_cast<qint64>(m.depthMax));
        metrics.insert(QStringLiteral("scalar_mean"), m.scalarMean);
        metrics.insert(QStringLiteral("scalar_abs_mean"), m.scalarAbsMean);
        metrics.insert(QStringLiteral("trit_density"), m.tritDensity);
        metrics.insert(QStringLiteral("trit_entropy"), m.tritEntropy);
        metrics.insert(QStringLiteral("clifford_energy"), m.cliffordEnergy);
        metrics.insert(QStringLiteral("beta1_proxy"), m.beta1Proxy);
        obj.insert(QStringLiteral("metrics"), metrics);
    }
    return obj;
}

ResultSummaryPanel::ResultSummaryPanel(QWidget *parent) : QFrame(parent) {
    setObjectName(QStringLiteral("ResultSummaryPanel"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 13, 14, 13);
    layout->setSpacing(0);

    auto *headingRow = new QWidget(this);
    auto *headingLayout = new QHBoxLayout(headingRow);
    headingLayout->setContentsMargins(0, 0, 0, 0);
    auto *heading = new QLabel(tr("RESULT SUMMARY"), headingRow);
    heading->setObjectName(QStringLiteral("SectionHeading"));
    auto *colTag = new QLabel(tr("column 3"), headingRow);
    colTag->setObjectName(QStringLiteral("ColumnTag"));
    headingLayout->addWidget(heading);
    headingLayout->addStretch(1);
    headingLayout->addWidget(colTag);
    layout->addWidget(headingRow);
    layout->addSpacing(11);

    m_metrics = new MetricsPanel(this);
    layout->addWidget(m_metrics);
    layout->addSpacing(10);

    m_jsonPreview = new QPlainTextEdit(this);
    m_jsonPreview->setObjectName(QStringLiteral("JsonPreview"));
    m_jsonPreview->setReadOnly(true);
    m_jsonPreview->setMaximumHeight(160);
    m_jsonPreview->setPlaceholderText(tr("json preview — appears once an archive is open"));
    layout->addWidget(m_jsonPreview);
    layout->addSpacing(10);

    m_rollbackNote =
        new QLabel(tr("Rollback unavailable: archive history cannot be validated safely."), this);
    m_rollbackNote->setObjectName(QStringLiteral("RollbackBanner"));
    m_rollbackNote->setWordWrap(true);
    layout->addWidget(m_rollbackNote);

    layout->addStretch(1);

    m_bottomActions = buildBottomActions();

    clear();
}

ResultSummaryPanel::~ResultSummaryPanel() {
    // bottomActionsWidget() is constructed parentless -- it's meant to be
    // reparented by whichever ThreeColumnPage calls setBottomActions(). If
    // that never happens, Qt's parent-deletes-children cleanup never
    // reaches it, so delete it here.
    if (m_bottomActions && !m_bottomActions->parentWidget())
        delete m_bottomActions;
}

QWidget *ResultSummaryPanel::buildBottomActions() {
    auto *container = makeCard(QStringLiteral("ResultBottomActions"));
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_copyJsonButton = new QPushButton(tr("Copy as JSON"), container);
    m_copyJsonButton->setObjectName(QStringLiteral("PrimaryButton"));
    connect(m_copyJsonButton, &QPushButton::clicked, this,
            &ResultSummaryPanel::copyAsJsonRequested);

    m_exportButton = new QPushButton(tr("Export stats report"), container);
    m_exportButton->setObjectName(QStringLiteral("SecondaryButton"));
    connect(m_exportButton, &QPushButton::clicked, this,
            &ResultSummaryPanel::exportStatsReportRequested);

    m_actionsCaption = new QLabel(container);
    m_actionsCaption->setObjectName(QStringLiteral("ActionsCaption"));
    m_actionsCaption->setWordWrap(true);
    m_actionsCaption->setAlignment(Qt::AlignCenter);

    layout->addWidget(m_copyJsonButton);
    layout->addWidget(m_exportButton);
    layout->addWidget(m_actionsCaption);
    return container;
}

void ResultSummaryPanel::setSnapshot(const ArchiveSnapshot &snapshot) {
    m_snapshot = snapshot;
    if (snapshot.metricsValid)
        m_metrics->setMetrics(snapshot.metrics);
    else
        m_metrics->setUnavailable();
    m_rollbackNote->setVisible(true);
    updateJsonPreview();
    m_copyJsonButton->setEnabled(true);
    m_exportButton->setEnabled(true);
    m_actionsCaption->setText(tr("both write outside the archive · the .crank is untouched"));
    setStyleProperty(this, "state", QStringLiteral("loaded"));
}

void ResultSummaryPanel::setPending() {
    m_snapshot = ArchiveSnapshot();
    m_metrics->setPending();
    m_rollbackNote->setVisible(false);
    m_jsonPreview->clear();
    m_copyJsonButton->setEnabled(false);
    m_exportButton->setEnabled(false);
    m_actionsCaption->setText(tr("available once an archive is open"));
    setStyleProperty(this, "state", QStringLiteral("pending"));
}

void ResultSummaryPanel::clear() {
    m_snapshot = ArchiveSnapshot();
    m_metrics->clear();
    m_rollbackNote->setVisible(false);
    m_jsonPreview->clear();
    m_copyJsonButton->setEnabled(false);
    m_exportButton->setEnabled(false);
    m_actionsCaption->setText(tr("available once an archive is open"));
    setStyleProperty(this, "state", QStringLiteral("empty"));
}

void ResultSummaryPanel::updateJsonPreview() {
    if (m_snapshot.isEmpty()) {
        m_jsonPreview->clear();
        return;
    }
    m_jsonPreview->setPlainText(
        QString::fromUtf8(QJsonDocument(snapshotToJson(m_snapshot)).toJson(QJsonDocument::Indented)));
}

} // namespace crankl_gui
