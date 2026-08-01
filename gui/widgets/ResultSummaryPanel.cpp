#include "widgets/ResultSummaryPanel.h"

#include "widgets/MetricsPanel.h"
#include "widgets/StyleUtil.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace crankl_gui {

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
    m_metrics->setMetrics(snapshot.metrics);
    m_rollbackNote->setVisible(true);
    m_copyJsonButton->setEnabled(true);
    m_exportButton->setEnabled(true);
    m_actionsCaption->setText(tr("both write outside the archive · the .crank is untouched"));
    setStyleProperty(this, "state", QStringLiteral("loaded"));
}

void ResultSummaryPanel::setPending() {
    m_metrics->setPending();
    m_rollbackNote->setVisible(false);
    m_copyJsonButton->setEnabled(false);
    m_exportButton->setEnabled(false);
    m_actionsCaption->setText(tr("available once an archive is open"));
    setStyleProperty(this, "state", QStringLiteral("pending"));
}

void ResultSummaryPanel::clear() {
    m_metrics->clear();
    m_rollbackNote->setVisible(false);
    m_copyJsonButton->setEnabled(false);
    m_exportButton->setEnabled(false);
    m_actionsCaption->setText(tr("available once an archive is open"));
    setStyleProperty(this, "state", QStringLiteral("empty"));
}

} // namespace crankl_gui
