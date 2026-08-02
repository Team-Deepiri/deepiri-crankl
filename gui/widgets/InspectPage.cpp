#include "widgets/InspectPage.h"

#include "widgets/ArchiveHealthHeader.h"
#include "widgets/ResultSummaryPanel.h"
#include "widgets/SlotBrowser.h"
#include "widgets/StyleUtil.h"
#include "widgets/ThreeColumnPage.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QResizeEvent>
#include <QSlider>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>

namespace crankl_gui {

namespace {
QString verifyStateJsonValue(VerifyState state) {
    switch (state) {
    case VerifyState::Pass:
        return QStringLiteral("pass");
    case VerifyState::Fail:
        return QStringLiteral("fail");
    case VerifyState::Unverified:
        return QStringLiteral("unverified");
    }
    return {};
}
} // namespace

InspectPage::InspectPage(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("InspectPage"));
    setAcceptDrops(true);

    // Content lives inside a vertical-only scroll area so short screens
    // scroll rather than blocking the window from shrinking; horizontal
    // adaptation is the ThreeColumnPage reflow, never a scrollbar.
    auto *content = new QWidget(this);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(makeVScroll(content, this));

    m_header = new ArchiveHealthHeader(content);
    connect(m_header, &ArchiveHealthHeader::verifyRequested, this, &InspectPage::verifyRequested);
    connect(m_header, &ArchiveHealthHeader::closeRequested, this, &InspectPage::closeRequested);
    connect(m_header, &ArchiveHealthHeader::jsonCopyRequested, this, &InspectPage::copyAsJson);
    layout->addWidget(m_header);

    m_frame = new ThreeColumnPage(content);
    layout->addWidget(m_frame, 1);

    m_frame->setLeftContent(buildLeftColumn());

    m_slotBrowser = new SlotBrowser(m_frame);
    connect(m_slotBrowser, &SlotBrowser::openArchiveRequested, this,
            &InspectPage::changeArchiveRequested);
    connect(m_slotBrowser, &SlotBrowser::recentRequested, this, &InspectPage::recentRequested);
    m_frame->setCenterContent(m_slotBrowser);

    m_resultSummary = new ResultSummaryPanel(m_frame);
    connect(m_resultSummary, &ResultSummaryPanel::copyAsJsonRequested, this,
            &InspectPage::copyAsJson);
    connect(m_resultSummary, &ResultSummaryPanel::exportStatsReportRequested, this,
            &InspectPage::exportStatsReport);
    m_frame->setRightContent(m_resultSummary);
    m_frame->setBottomActions(m_resultSummary->bottomActionsWidget());

    applyEmptyState(true);
}

QWidget *InspectPage::buildLeftColumn() {
    auto *panel = new QWidget;
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // --- INPUT card. Accented "· REQUIRED" styling while empty (4a).
    m_inputCard = makeCard(QStringLiteral("InputCard"), panel);
    auto *inputLayout = new QVBoxLayout(m_inputCard);
    inputLayout->setContentsMargins(14, 13, 14, 13);
    inputLayout->setSpacing(9);

    auto *inputHeadingRow = new QWidget(m_inputCard);
    auto *inputHeadingLayout = new QHBoxLayout(inputHeadingRow);
    inputHeadingLayout->setContentsMargins(0, 0, 0, 0);
    m_inputHeading = new QLabel(inputHeadingRow);
    m_inputHeading->setObjectName(QStringLiteral("SectionHeading"));
    auto *inputColTag = new QLabel(tr("column 1"), inputHeadingRow);
    inputColTag->setObjectName(QStringLiteral("ColumnTag"));
    inputHeadingLayout->addWidget(m_inputHeading);
    inputHeadingLayout->addStretch(1);
    inputHeadingLayout->addWidget(inputColTag);
    inputLayout->addWidget(inputHeadingRow);

    m_pathBox = makeCard(QStringLiteral("PathBox"), m_inputCard);
    auto *pathBoxLayout = new QHBoxLayout(m_pathBox);
    pathBoxLayout->setContentsMargins(9, 7, 9, 7);
    pathBoxLayout->setSpacing(7);
    m_pathLabel = new QLabel(m_pathBox);
    m_pathLabel->setObjectName(QStringLiteral("PathLabel"));
    m_changeButton = new QPushButton(tr("Change"), m_pathBox);
    m_changeButton->setObjectName(QStringLiteral("LinkButton"));
    m_changeButton->setCursor(Qt::PointingHandCursor);
    connect(m_changeButton, &QPushButton::clicked, this, &InspectPage::changeArchiveRequested);
    pathBoxLayout->addWidget(m_pathLabel, 1);
    pathBoxLayout->addWidget(m_changeButton);
    inputLayout->addWidget(m_pathBox);

    m_openButton = new QPushButton(tr("Open archive…"), m_inputCard);
    m_openButton->setObjectName(QStringLiteral("PrimaryButton"));
    connect(m_openButton, &QPushButton::clicked, this, &InspectPage::changeArchiveRequested);
    inputLayout->addWidget(m_openButton);

    auto *requiresNote =
        new QLabel(tr("requires: .crank archive\nproduces: nothing on disk"), m_inputCard);
    requiresNote->setObjectName(QStringLiteral("MutedMono"));
    inputLayout->addWidget(requiresNote);
    layout->addWidget(m_inputCard);

    // --- PARAMETERS card. Disabled, not hidden, while empty (4a):
    // "controls stay in place and enable the moment an archive is open".
    m_paramsCard = makeCard(QStringLiteral("ParamsCard"), panel);
    auto *paramsLayout = new QVBoxLayout(m_paramsCard);
    paramsLayout->setContentsMargins(14, 13, 14, 13);
    paramsLayout->setSpacing(13);

    auto *paramsHeadingRow = new QWidget(m_paramsCard);
    auto *paramsHeadingLayout = new QHBoxLayout(paramsHeadingRow);
    paramsHeadingLayout->setContentsMargins(0, 0, 0, 0);
    auto *paramsHeading = new QLabel(tr("PARAMETERS"), paramsHeadingRow);
    paramsHeading->setObjectName(QStringLiteral("SectionHeading"));
    m_paramsStatus = new QLabel(paramsHeadingRow);
    m_paramsStatus->setObjectName(QStringLiteral("ColumnTag"));
    paramsHeadingLayout->addWidget(paramsHeading);
    paramsHeadingLayout->addStretch(1);
    paramsHeadingLayout->addWidget(m_paramsStatus);
    paramsLayout->addWidget(paramsHeadingRow);

    auto *slotGroup = new QWidget(m_paramsCard);
    auto *slotGroupLayout = new QVBoxLayout(slotGroup);
    slotGroupLayout->setContentsMargins(0, 0, 0, 0);
    slotGroupLayout->setSpacing(6);
    auto *slotLabel = new QLabel(tr("Slot"), slotGroup);
    slotLabel->setObjectName(QStringLiteral("ParamLabel"));
    slotGroupLayout->addWidget(slotLabel);
    auto *slotRow = new QWidget(slotGroup);
    auto *slotRowLayout = new QHBoxLayout(slotRow);
    slotRowLayout->setContentsMargins(0, 0, 0, 0);
    slotRowLayout->setSpacing(8);
    m_slotSpin = new QSpinBox(slotRow);
    m_slotSpin->setMinimumWidth(72);
    m_slotSlider = new QSlider(Qt::Horizontal, slotRow);
    slotRowLayout->addWidget(m_slotSpin);
    slotRowLayout->addWidget(m_slotSlider, 1);
    slotGroupLayout->addWidget(slotRow);
    paramsLayout->addWidget(slotGroup);
    connect(m_slotSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_slotSlider,
            &QSlider::setValue);
    connect(m_slotSlider, &QSlider::valueChanged, m_slotSpin, &QSpinBox::setValue);
    connect(m_slotSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int value) { m_slotBrowser->setSlotIndex(value); });

    auto *viewGroupWidget = new QWidget(m_paramsCard);
    auto *viewGroupLayout = new QVBoxLayout(viewGroupWidget);
    viewGroupLayout->setContentsMargins(0, 0, 0, 0);
    viewGroupLayout->setSpacing(6);
    auto *viewLabel = new QLabel(tr("Block view"), viewGroupWidget);
    viewLabel->setObjectName(QStringLiteral("ParamLabel"));
    viewGroupLayout->addWidget(viewLabel);
    auto *segmented = makeCard(QStringLiteral("SegmentedControl"), viewGroupWidget);
    auto *segmentedLayout = new QHBoxLayout(segmented);
    segmentedLayout->setContentsMargins(0, 0, 0, 0);
    segmentedLayout->setSpacing(0);
    m_heatmapButton = new QPushButton(tr("Heatmap"), segmented);
    m_heatmapButton->setObjectName(QStringLiteral("SegmentButton"));
    m_heatmapButton->setCheckable(true);
    m_heatmapButton->setChecked(true);
    m_numericButton = new QPushButton(tr("Numeric"), segmented);
    m_numericButton->setObjectName(QStringLiteral("SegmentButton"));
    m_numericButton->setCheckable(true);
    auto *viewButtonGroup = new QButtonGroup(segmented);
    viewButtonGroup->setExclusive(true);
    viewButtonGroup->addButton(m_heatmapButton);
    viewButtonGroup->addButton(m_numericButton);
    connect(m_numericButton, &QPushButton::toggled, this,
            [this](bool checked) { m_slotBrowser->setNumericMode(checked); });
    segmentedLayout->addWidget(m_heatmapButton, 1);
    segmentedLayout->addWidget(m_numericButton, 1);
    viewGroupLayout->addWidget(segmented);
    paramsLayout->addWidget(viewGroupWidget);

    auto *wordGroupWidget = new QWidget(m_paramsCard);
    auto *wordGroupLayout = new QVBoxLayout(wordGroupWidget);
    wordGroupLayout->setContentsMargins(0, 0, 0, 0);
    wordGroupLayout->setSpacing(6);
    auto *wordLabel = new QLabel(tr("Word display"), wordGroupWidget);
    wordLabel->setObjectName(QStringLiteral("ParamLabel"));
    wordGroupLayout->addWidget(wordLabel);
    m_wordDisplayCombo = new QComboBox(wordGroupWidget);
    m_wordDisplayCombo->addItem(tr("hex + bits"));
    wordGroupLayout->addWidget(m_wordDisplayCombo);
    paramsLayout->addWidget(wordGroupWidget);

    m_reVerifyOnOpenCheck = new QCheckBox(tr("Re-verify on open"), m_paramsCard);
    m_reVerifyOnOpenCheck->setChecked(true);
    m_reVerifyOnOpenCheck->setToolTip(
        tr("Reserved for a future phase — this build always verifies on open."));
    paramsLayout->addWidget(m_reVerifyOnOpenCheck);

    paramsLayout->addStretch(1);

    m_paramsFooter = new QLabel(m_paramsCard);
    m_paramsFooter->setObjectName(QStringLiteral("MutedMono"));
    m_paramsFooter->setWordWrap(true);
    paramsLayout->addWidget(m_paramsFooter);

    layout->addWidget(m_paramsCard, 1);
    return panel;
}

void InspectPage::applyEmptyState(bool empty) {
    m_inputHeading->setText(empty ? tr("INPUT · REQUIRED") : tr("INPUT"));
    setStyleProperty(m_inputCard, "required", empty);
    setStyleProperty(m_inputHeading, "required", empty);
    setStyleProperty(m_pathBox, "empty", empty);
    m_pathLabel->setText(empty ? tr("no file selected") : m_snapshot.path);
    m_pathLabel->setToolTip(empty ? QString() : m_snapshot.path);
    setStyleProperty(m_pathLabel, "empty", empty);
    m_changeButton->setVisible(!empty);
    m_openButton->setVisible(empty);

    m_paramsStatus->setText(empty ? tr("disabled") : QString());
    m_slotSpin->setEnabled(!empty);
    m_slotSlider->setEnabled(!empty);
    m_heatmapButton->setEnabled(!empty);
    m_numericButton->setEnabled(!empty);
    // Only one word-display mode exists in Phase 1, so the combo stays
    // disabled even when loaded; it still greys further with the card.
    m_wordDisplayCombo->setEnabled(false);
    m_reVerifyOnOpenCheck->setEnabled(!empty);
    setStyleProperty(m_paramsCard, "disabled", empty);
    m_paramsFooter->setText(
        empty ? tr("controls stay in place and enable the moment an archive is open")
              : tr("parameters only change what is displayed — none of them can alter the file"));
}

void InspectPage::setSnapshot(const ArchiveSnapshot &snapshot) {
    m_snapshot = snapshot;
    m_header->setSnapshot(snapshot);

    const int maxSlot =
        snapshot.crankWords.empty() ? 0 : static_cast<int>(snapshot.crankWords.size()) - 1;
    m_slotSpin->setRange(0, maxSlot);
    m_slotSlider->setRange(0, maxSlot);
    m_slotSpin->setValue(0);

    if (snapshot.verifyState == VerifyState::Pass) {
        m_slotBrowser->setSnapshot(snapshot);
        m_resultSummary->setSnapshot(snapshot);
        applyEmptyState(false);
    } else {
        // Verify-fail: metrics and the slot browser stay hidden (1d) --
        // the header carries the fail state; columns fall back to empty.
        m_slotBrowser->clear();
        m_resultSummary->clear();
        applyEmptyState(false);
        m_pathLabel->setText(snapshot.path);
    }
}

void InspectPage::clearSnapshot() {
    m_snapshot = ArchiveSnapshot{};
    m_header->setSnapshot(m_snapshot);
    m_slotSpin->setRange(0, 0);
    m_slotSlider->setRange(0, 0);
    m_slotBrowser->clear();
    m_resultSummary->clear();
    applyEmptyState(true);
}

void InspectPage::debugDump() {
    qDebug() << "page w" << width() << "frame w" << m_frame->width() << "frameMin"
             << m_frame->minimumSizeHint().width() << "slotBrowser w" << m_slotBrowser->width()
             << "slotBrowserMin" << m_slotBrowser->minimumSizeHint().width() << "summary w"
             << m_resultSummary->width() << "summary x" << m_resultSummary->x() << "summary vis"
             << m_resultSummary->isVisible();
}

void InspectPage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    // This page's width is honest (it's outside the min-clamped scroll
    // content), so it drives both responsive behaviors: the header's
    // compact mode and the frame's narrow reflow. 40 = page side margins.
    const int available = width() - 40;
    m_header->setCompact(available < 900);
    m_slotBrowser->setCondensed(available < 990);
    m_frame->setAvailableWidth(available);
}

void InspectPage::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void InspectPage::dropEvent(QDropEvent *event) {
    const auto urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;
    const QString path = urls.first().toLocalFile();
    if (path.isEmpty())
        return;
    event->acceptProposedAction();
    Q_EMIT pathDropped(path);
}

QJsonObject InspectPage::buildReportJson() const {
    QJsonObject obj;
    obj.insert(QStringLiteral("path"), m_snapshot.path);
    obj.insert(QStringLiteral("verify"), verifyStateJsonValue(m_snapshot.verifyState));
    obj.insert(QStringLiteral("n_slots"), static_cast<qint64>(m_snapshot.metrics.nSlots));
    obj.insert(QStringLiteral("depth_min"), static_cast<int>(m_snapshot.metrics.depthMin));
    obj.insert(QStringLiteral("depth_max"), static_cast<int>(m_snapshot.metrics.depthMax));
    obj.insert(QStringLiteral("scalar_mean"), m_snapshot.metrics.scalarMean);
    obj.insert(QStringLiteral("scalar_abs_mean"), m_snapshot.metrics.scalarAbsMean);
    obj.insert(QStringLiteral("trit_density"), m_snapshot.metrics.tritDensity);
    obj.insert(QStringLiteral("trit_entropy"), m_snapshot.metrics.tritEntropy);
    obj.insert(QStringLiteral("clifford_energy"), m_snapshot.metrics.cliffordEnergy);
    obj.insert(QStringLiteral("beta1_proxy"), m_snapshot.metrics.beta1Proxy);
    if (m_snapshot.metadata) {
        QJsonObject metadata;
        metadata.insert(QStringLiteral("model"), m_snapshot.metadata->modelName);
        metadata.insert(QStringLiteral("hash"), m_snapshot.metadata->sourceHash);
        obj.insert(QStringLiteral("metadata"), metadata);
    }
    return obj;
}

void InspectPage::copyAsJson() {
    if (m_snapshot.isEmpty())
        return;
    const QJsonDocument doc(buildReportJson());
    QApplication::clipboard()->setText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
}

void InspectPage::exportStatsReport() {
    if (m_snapshot.isEmpty())
        return;
    const QString suggested = m_snapshot.path + QStringLiteral(".stats.json");
    const QString path = QFileDialog::getSaveFileName(this, tr("Export stats report"), suggested,
                                                      tr("JSON (*.json)"));
    if (path.isEmpty())
        return;

    // A user-initiated export must never fail silently: the user picked a
    // destination and expects a file there. Both the open and the write are
    // reported, and a partial write is treated as a failure rather than left
    // on disk unannounced.
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, tr("Export failed"),
                              tr("Could not open %1 for writing:\n%2")
                                  .arg(QDir::toNativeSeparators(path), file.errorString()));
        return;
    }

    const QByteArray payload = QJsonDocument(buildReportJson()).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size() || !file.flush()) {
        QMessageBox::critical(
            this, tr("Export failed"),
            tr("Could not write %1:\n%2").arg(QDir::toNativeSeparators(path), file.errorString()));
        file.close();
        return;
    }
    file.close();
}

} // namespace crankl_gui
