#include "widgets/ArchiveHealthHeader.h"

#include "widgets/StyleUtil.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace crankl_gui {

namespace {
QString verifyBadgeText(VerifyState state, bool empty) {
    if (empty)
        return QObject::tr("NO ARCHIVE");
    switch (state) {
    case VerifyState::Pass:
        return QObject::tr("VERIFY PASS");
    case VerifyState::Fail:
        return QObject::tr("VERIFY FAIL");
    case VerifyState::Unverified:
        return QObject::tr("VERIFYING");
    }
    return {};
}

QString verifyStateProperty(VerifyState state, bool empty) {
    if (empty)
        return QStringLiteral("empty");
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

ArchiveHealthHeader::ArchiveHealthHeader(QWidget *parent) : QFrame(parent) {
    setObjectName(QStringLiteral("ArchiveHealthHeader"));
    setAttribute(Qt::WA_StyledBackground, true);
    buildUi();
    applyState();
}

void ArchiveHealthHeader::buildUi() {
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Zone 1: state -- 4px colored rail + badge + timestamp, fixed 112px.
    m_stateRail = new QFrame(this);
    m_stateRail->setObjectName(QStringLiteral("HealthStateRail"));
    m_stateRail->setAttribute(Qt::WA_StyledBackground, true);
    m_stateRail->setFixedWidth(4);
    root->addWidget(m_stateRail);

    auto *stateZone = makeCard(QStringLiteral("HealthStateZone"), this);
    stateZone->setFixedWidth(112);
    auto *stateLayout = new QVBoxLayout(stateZone);
    stateLayout->setContentsMargins(12, 12, 12, 12);
    stateLayout->setSpacing(6);
    stateLayout->setAlignment(Qt::AlignCenter);
    m_stateBadge = new QLabel(stateZone);
    m_stateBadge->setObjectName(QStringLiteral("HealthStateBadge"));
    m_stateBadge->setAlignment(Qt::AlignCenter);
    m_stateTimestamp = new QLabel(stateZone);
    m_stateTimestamp->setObjectName(QStringLiteral("HealthStateTimestamp"));
    m_stateTimestamp->setAlignment(Qt::AlignCenter);
    stateLayout->addWidget(m_stateBadge);
    stateLayout->addWidget(m_stateTimestamp);
    root->addWidget(stateZone);

    // Zone 2: identity -- filename, then path · size. Only stretching zone.
    auto *identityZone = makeCard(QStringLiteral("HealthIdentityZone"), this);
    auto *identityLayout = new QVBoxLayout(identityZone);
    identityLayout->setContentsMargins(14, 12, 14, 12);
    identityLayout->setSpacing(4);
    m_identityName = new QLabel(identityZone);
    m_identityName->setObjectName(QStringLiteral("HealthIdentityName"));
    m_identityPath = new QLabel(identityZone);
    m_identityPath->setObjectName(QStringLiteral("HealthIdentityPath"));
    // Informational; may be clipped rather than widening the header's
    // minimum (it's hidden entirely in compact mode anyway).
    m_identityPath->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    identityLayout->addWidget(m_identityName);
    identityLayout->addWidget(m_identityPath);
    identityLayout->addStretch(1);
    root->addWidget(identityZone, 1);

    // Zone 3: counts -- exactly four cells, fixed order and widths; every
    // cell always renders a value. Tooltip texts are the design's 5b set.
    auto addCount = [&](const QString &label, int width, const QString &tooltip) {
        auto *cell = makeCard(QStringLiteral("HealthCountCell"), this);
        cell->setMinimumWidth(width);
        cell->setToolTip(tooltip);
        auto *cellLayout = new QVBoxLayout(cell);
        cellLayout->setContentsMargins(14, 12, 14, 12);
        cellLayout->setSpacing(4);
        auto *labelWidget = new QLabel(label, cell);
        labelWidget->setObjectName(QStringLiteral("HealthCountLabel"));
        auto *valueWidget = new QLabel(cell);
        valueWidget->setObjectName(QStringLiteral("HealthCountValue"));
        cellLayout->addWidget(labelWidget);
        cellLayout->addWidget(valueWidget);
        cellLayout->addStretch(1);
        root->addWidget(cell);
        return valueWidget;
    };
    m_countNSlots =
        addCount(tr("n_slots"), 100,
                 tr("Number of crank words in the archive. One slot holds one 64-bit word, "
                    "decoding to an 8×8 block."));
    m_countDepth = addCount(tr("depth"), 88,
                             tr("Lowest and highest encoding depth across all slots — depth_min "
                                "and depth_max."));
    m_countMetadata =
        addCount(tr("metadata"), 96,
                 tr("Optional provenance footer: model name and source hash. Absence is normal "
                    "and is not a defect."));
    m_countHistory = addCount(tr("history"), 110,
                               tr("Rollback unavailable: archive history cannot be validated "
                                  "safely."));

    // Zone 4: actions -- read-only, forever.
    auto *actionsZone = makeCard(QStringLiteral("HealthActionsZone"), this);
    auto *actionsLayout = new QHBoxLayout(actionsZone);
    actionsLayout->setContentsMargins(14, 12, 14, 12);
    actionsLayout->setSpacing(6);
    m_reVerifyButton = new QPushButton(tr("Re-run verify"), actionsZone);
    m_reVerifyButton->setObjectName(QStringLiteral("HeaderActionButton"));
    connect(m_reVerifyButton, &QPushButton::clicked, this, &ArchiveHealthHeader::verifyRequested);
    m_copyJsonButton = new QPushButton(tr("Copy as JSON"), actionsZone);
    m_copyJsonButton->setObjectName(QStringLiteral("HeaderActionButton"));
    connect(m_copyJsonButton, &QPushButton::clicked, this, &ArchiveHealthHeader::jsonCopyRequested);
    m_closeButton = new QToolButton(actionsZone);
    m_closeButton->setObjectName(QStringLiteral("HeaderCloseButton"));
    m_closeButton->setText(QStringLiteral("×"));
    m_closeButton->setFixedSize(26, 26);
    connect(m_closeButton, &QToolButton::clicked, this, &ArchiveHealthHeader::closeRequested);
    actionsLayout->addWidget(m_reVerifyButton);
    actionsLayout->addWidget(m_copyJsonButton);
    actionsLayout->addWidget(m_closeButton);
    root->addWidget(actionsZone);
}

void ArchiveHealthHeader::setSnapshot(const ArchiveSnapshot &snapshot) {
    m_snapshot = snapshot;
    m_verifyState = snapshot.verifyState;
    applyState();
}

void ArchiveHealthHeader::setVerifyState(VerifyState state) {
    m_verifyState = state;
    applyState();
}

void ArchiveHealthHeader::setCompact(bool compact) {
    if (m_compact == compact)
        return;
    m_compact = compact;
    applyCompact();
}

void ArchiveHealthHeader::applyState() {
    const bool empty = m_snapshot.isEmpty();
    const QString stateProp = verifyStateProperty(m_verifyState, empty);

    m_stateBadge->setText(verifyBadgeText(m_verifyState, empty));
    setStyleProperty(this, "state", stateProp);
    setStyleProperty(m_stateRail, "state", stateProp);
    setStyleProperty(m_stateBadge, "state", stateProp);

    m_stateTimestamp->setText(!empty && m_snapshot.verifiedAt.isValid()
                                   ? m_snapshot.verifiedAt.toString(QStringLiteral("HH:mm:ss"))
                                   : QStringLiteral("—"));

    if (empty) {
        m_identityName->setText(tr("no archive open"));
        m_identityPath->setText(QStringLiteral("—"));
    } else {
        m_identityName->setText(m_snapshot.fileName);
        const double kb = m_snapshot.byteSize / 1024.0;
        const QString sizeText = kb < 1024.0
                                      ? tr("%1 KB").arg(QString::number(kb, 'f', 1))
                                      : tr("%1 MB").arg(QString::number(kb / 1024.0, 'f', 2));
        m_identityPath->setText(tr("%1 · %2").arg(m_snapshot.path, sizeText));
    }
    setStyleProperty(m_identityName, "empty", empty);
    setStyleProperty(m_identityPath, "empty", empty);

    const bool haveData = !empty && m_verifyState == VerifyState::Pass;
    const QString dash = QStringLiteral("—");
    m_countNSlots->setText(haveData ? QString::number(m_snapshot.metrics.nSlots) : dash);
    m_countDepth->setText(haveData ? tr("%1–%2")
                                          .arg(m_snapshot.metrics.depthMin)
                                          .arg(m_snapshot.metrics.depthMax)
                                    : dash);
    m_countMetadata->setText(!haveData            ? dash
                              : m_snapshot.metadata ? tr("present")
                                                    : tr("none"));
    m_countHistory->setText(!haveData                     ? dash
                             : m_snapshot.historyAvailable ? tr("available")
                                                           : tr("unavailable"));
    // "none" is a value in muted grey; "unavailable" is the amber warning.
    setStyleProperty(m_countMetadata, "tone",
                     haveData && !m_snapshot.metadata ? QStringLiteral("muted")
                     : haveData                        ? QStringLiteral("value")
                                                       : QStringLiteral("dim"));
    setStyleProperty(m_countHistory, "tone",
                     haveData ? QStringLiteral("amber") : QStringLiteral("dim"));
    setStyleProperty(m_countNSlots, "tone",
                     haveData ? QStringLiteral("value") : QStringLiteral("dim"));
    setStyleProperty(m_countDepth, "tone",
                     haveData ? QStringLiteral("value") : QStringLiteral("dim"));

    m_reVerifyButton->setEnabled(!empty);
    m_copyJsonButton->setEnabled(haveData);
    m_closeButton->setEnabled(!empty);
}

void ArchiveHealthHeader::applyCompact() {
    setStyleProperty(this, "compact", m_compact);
    m_reVerifyButton->setText(m_compact ? QStringLiteral("↻") : tr("Re-run verify"));
    m_copyJsonButton->setVisible(!m_compact);
    m_identityPath->setVisible(!m_compact);
}

} // namespace crankl_gui
