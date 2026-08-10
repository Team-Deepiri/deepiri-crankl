#include "widgets/HomePage.h"

#include "widgets/ArchiveHealthHeader.h"
#include "widgets/StyleUtil.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QPushButton>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace crankl_gui {

namespace {

QLabel *makeRollbackBanner(QWidget *parent) {
    auto *banner = new QLabel(QObject::tr("Rollback unavailable: archive history cannot be "
                                          "validated safely."),
                              parent);
    banner->setObjectName(QStringLiteral("RollbackBanner"));
    banner->setWordWrap(true);
    return banner;
}

QPushButton *makeActionButton(const QString &text, bool enabled, QWidget *parent) {
    auto *button = new QPushButton(text, parent);
    button->setEnabled(enabled);
    if (!enabled)
        button->setToolTip(QObject::tr("Not available yet."));
    return button;
}

} // namespace

HomePage::HomePage(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("HomePage"));
    setAcceptDrops(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);
    // Vertical-only scroll so short screens scroll instead of blocking the
    // window's minimum height (same treatment as InspectPage).
    layout->addWidget(makeVScroll(m_stack, this));

    m_emptyPanel = buildEmptyStatePanel();
    m_selectedPanel = buildSelectedStatePanel();
    m_unsupportedPanel = buildUnsupportedPanel();
    m_stack->addWidget(m_emptyPanel);
    m_stack->addWidget(m_selectedPanel);
    m_stack->addWidget(m_unsupportedPanel);

    showEmptyState();
}

QWidget *HomePage::buildEmptyStatePanel() {
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);

    auto *dropZone = makeCard(QStringLiteral("DropZone"), panel);
    auto *dropLayout = new QVBoxLayout(dropZone);
    dropLayout->setAlignment(Qt::AlignCenter);

    auto *title = new QLabel(tr("Drop a .crank archive"), dropZone);
    title->setObjectName(QStringLiteral("DropZoneTitle"));
    title->setAlignment(Qt::AlignCenter);

    auto *subtitle =
        new QLabel(tr("This build reads archives. Weight files are accepted at the drop "
                      "target but route to an explanation, not an import."),
                   dropZone);
    subtitle->setObjectName(QStringLiteral("DropZoneSubtitle"));
    subtitle->setWordWrap(true);
    subtitle->setAlignment(Qt::AlignCenter);

    auto *buttonsRow = new QWidget(dropZone);
    auto *buttonsLayout = new QHBoxLayout(buttonsRow);
    auto *openArchiveButton = new QPushButton(tr("Open archive…"), buttonsRow);
    openArchiveButton->setObjectName(QStringLiteral("PrimaryButton"));
    connect(openArchiveButton, &QPushButton::clicked, this, &HomePage::openArchiveRequested);
    auto *openWeightsButton = new QPushButton(tr("Open weights…"), buttonsRow);
    connect(openWeightsButton, &QPushButton::clicked, this, &HomePage::openWeightsRequested);
    buttonsLayout->addWidget(openArchiveButton);
    buttonsLayout->addWidget(openWeightsButton);

    auto *hint = new QLabel(tr("accepts .crank · read via crankl_cran_read()"), dropZone);
    hint->setObjectName(QStringLiteral("DropZoneHint"));
    hint->setAlignment(Qt::AlignCenter);

    dropLayout->addWidget(title);
    dropLayout->addWidget(subtitle);
    dropLayout->addWidget(buttonsRow);
    dropLayout->addWidget(hint);
    layout->addWidget(dropZone, 1);

    auto *quickActions = new QWidget(panel);
    auto *quickLayout = new QHBoxLayout(quickActions);
    auto addQuickCard = [&](const QString &title_, const QString &subtitle_, bool enabled) {
        auto *card = makeCard(QStringLiteral("QuickActionCard"), quickActions);
        card->setProperty("enabled_card", enabled);
        auto *cardLayout = new QVBoxLayout(card);
        auto *cardTitle = new QLabel(title_, card);
        cardTitle->setObjectName(QStringLiteral("QuickActionTitle"));
        auto *cardSubtitle = new QLabel(subtitle_, card);
        cardSubtitle->setObjectName(QStringLiteral("QuickActionSubtitle"));
        cardSubtitle->setWordWrap(true);
        cardLayout->addWidget(cardTitle);
        cardLayout->addWidget(cardSubtitle);
        quickLayout->addWidget(card);
        return card;
    };
    addQuickCard(tr("Inspect archive"),
                 tr("Metrics, identity, slot browser and pass/fail verify on one page."), true);
    addQuickCard(tr("Pack weights"), tr("Not available yet."), false);
    addQuickCard(tr("Compare two archives"), tr("Not available yet."), false);
    layout->addWidget(quickActions);

    auto *readOnlyBanner = makeCard(QStringLiteral("ReadOnlyBanner"), panel);
    auto *bannerLayout = new QHBoxLayout(readOnlyBanner);
    auto *bannerLabel = new QLabel(tr("READ-ONLY BUILD"), readOnlyBanner);
    bannerLabel->setObjectName(QStringLiteral("ReadOnlyBannerLabel"));
    auto *bannerText = new QLabel(
        tr("No archive file is created, modified, or deleted anywhere in this build — there "
           "is no code path to write_cran() and none to crankl_peel_stack()."),
        readOnlyBanner);
    bannerText->setWordWrap(true);
    bannerLayout->addWidget(bannerLabel);
    bannerLayout->addWidget(bannerText, 1);
    layout->addWidget(readOnlyBanner);

    return panel;
}

QWidget *HomePage::buildSelectedStatePanel() {
    auto *panel = new QWidget(this);
    auto *outer = new QHBoxLayout(panel);

    auto *left = new QWidget(panel);
    auto *leftLayout = new QVBoxLayout(left);

    m_overviewHeader = new ArchiveHealthHeader(left);
    connect(m_overviewHeader, &ArchiveHealthHeader::verifyRequested, this,
            &HomePage::reVerifyRequested);
    connect(m_overviewHeader, &ArchiveHealthHeader::closeRequested, this,
            &HomePage::closeArchiveRequested);
    leftLayout->addWidget(m_overviewHeader);
    leftLayout->addWidget(makeRollbackBanner(left));

    auto *actionsGroup = makeCard(QStringLiteral("GroupCard"), left);
    auto *actionsGroupLayout = new QVBoxLayout(actionsGroup);
    auto *actionsHeading = new QLabel(tr("ACTIONS"), actionsGroup);
    actionsHeading->setObjectName(QStringLiteral("SectionHeading"));
    actionsGroupLayout->addWidget(actionsHeading);
    actionsGroupLayout->addWidget(buildActionButtonsGrid());
    auto *actionsNote =
        new QLabel(tr("Dimmed controls keep their position and label so the shell doesn't "
                      "rearrange later."),
                   actionsGroup);
    actionsNote->setWordWrap(true);
    actionsNote->setObjectName(QStringLiteral("MutedNote"));
    actionsGroupLayout->addWidget(actionsNote);
    leftLayout->addWidget(actionsGroup, 1);

    outer->addWidget(left, 1);

    auto *right = new QWidget(panel);
    right->setFixedWidth(300);
    auto *rightLayout = new QVBoxLayout(right);

    auto *identityGroup = makeCard(QStringLiteral("GroupCard"), right);
    auto *identityLayout = new QVBoxLayout(identityGroup);
    auto *identityHeading = new QLabel(tr("IDENTITY"), identityGroup);
    identityHeading->setObjectName(QStringLiteral("SectionHeading"));
    m_identityModel = new QLabel(right);
    m_identityHash = new QLabel(right);
    auto *identityNote =
        new QLabel(tr("No provenance footer is normal, not a failure — "
                      "crankl_cran_read_metadata() returned CRANKL_ERR_NO_METADATA."),
                   identityGroup);
    identityNote->setWordWrap(true);
    identityNote->setObjectName(QStringLiteral("MutedNote"));
    identityLayout->addWidget(identityHeading);
    identityLayout->addWidget(m_identityModel);
    identityLayout->addWidget(m_identityHash);
    identityLayout->addWidget(identityNote);
    rightLayout->addWidget(identityGroup);

    auto *lifecycleGroup = makeCard(QStringLiteral("GroupCard"), right);
    auto *lifecycleLayout = new QVBoxLayout(lifecycleGroup);
    auto *lifecycleHeading = new QLabel(tr("ADAPTER LIFECYCLE"), lifecycleGroup);
    lifecycleHeading->setObjectName(QStringLiteral("SectionHeading"));
    m_lifecycleLog = new QLabel(lifecycleGroup);
    m_lifecycleLog->setWordWrap(true);
    m_lifecycleLog->setObjectName(QStringLiteral("LifecycleLog"));
    auto *lifecycleNote =
        new QLabel(tr("Everything on screen is a copy. No live pointers into the mapping, "
                      "and only one archive is ever open at a time."),
                   lifecycleGroup);
    lifecycleNote->setWordWrap(true);
    lifecycleNote->setObjectName(QStringLiteral("MutedNote"));
    lifecycleLayout->addWidget(lifecycleHeading);
    lifecycleLayout->addWidget(m_lifecycleLog);
    lifecycleLayout->addWidget(lifecycleNote);
    rightLayout->addWidget(lifecycleGroup, 1);

    outer->addWidget(right);

    return panel;
}

QWidget *HomePage::buildActionButtonsGrid() {
    auto *grid = new QWidget(this);
    auto *layout = new QGridLayout(grid);

    auto *inspectButton = makeActionButton(tr("Inspect details"), true, grid);
    connect(inspectButton, &QPushButton::clicked, this, &HomePage::inspectRequested);
    auto *reVerifyButton = makeActionButton(tr("Re-verify"), true, grid);
    connect(reVerifyButton, &QPushButton::clicked, this, &HomePage::reVerifyRequested);

    layout->addWidget(inspectButton, 0, 0);
    layout->addWidget(reVerifyButton, 0, 1);
    layout->addWidget(makeActionButton(tr("Optimize"), false, grid), 1, 0);
    layout->addWidget(makeActionButton(tr("Compare"), false, grid), 1, 1);
    layout->addWidget(makeActionButton(tr("Peel history"), false, grid), 2, 0);
    layout->addWidget(makeActionButton(tr("Bind"), false, grid), 2, 1);
    layout->addWidget(makeActionButton(tr("Run forward"), false, grid), 3, 0);
    layout->addWidget(makeActionButton(tr("Unpack"), false, grid), 3, 1);

    return grid;
}

QWidget *HomePage::buildUnsupportedPanel() {
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->addStretch(1);

    auto *badge = new QLabel(tr("NOT SUPPORTED"), panel);
    badge->setObjectName(QStringLiteral("UnsupportedBadge"));
    badge->setAlignment(Qt::AlignCenter);

    m_unsupportedFileName = new QLabel(panel);
    m_unsupportedFileName->setAlignment(Qt::AlignCenter);
    m_unsupportedFileName->setObjectName(QStringLiteral("UnsupportedFileName"));

    auto *explanation =
        new QLabel(tr("That file was accepted at the drop target but this build cannot read "
                      "weight files — only .crank archives. Nothing was opened and nothing "
                      "was written."),
                   panel);
    explanation->setWordWrap(true);
    explanation->setAlignment(Qt::AlignCenter);

    auto *backButton = new QPushButton(tr("Open a .crank instead…"), panel);
    connect(backButton, &QPushButton::clicked, this, &HomePage::openArchiveRequested);

    layout->addWidget(badge);
    layout->addWidget(m_unsupportedFileName);
    layout->addWidget(explanation);
    layout->addWidget(backButton, 0, Qt::AlignCenter);
    layout->addStretch(1);

    return panel;
}

void HomePage::showEmptyState() {
    m_stack->setCurrentWidget(m_emptyPanel);
}

void HomePage::showArchive(const ArchiveSnapshot &snapshot) {
    m_overviewHeader->setSnapshot(snapshot);

    if (snapshot.metadataState == MetadataState::Present && snapshot.metadata.has_value()) {
        m_identityModel->setText(tr("model name\n%1").arg(snapshot.metadata->modelName));
        m_identityHash->setText(tr("source hash\n%1").arg(snapshot.metadata->sourceHash));
    } else if (snapshot.metadataState == MetadataState::Error) {
        // Not the same as absence -- say so, and say why.
        m_identityModel->setText(
            tr("model name\ncould not be read: %1").arg(snapshot.metadataError));
        m_identityHash->setText(tr("source hash\ncould not be read"));
    } else {
        m_identityModel->setText(tr("model name\nno metadata"));
        m_identityHash->setText(tr("source hash\nno metadata"));
    }

    const QString completedAt = snapshot.verifiedAt.isValid()
                                    ? snapshot.verifiedAt.toString(QStringLiteral("HH:mm:ss.zzz"))
                                    : QString();
    m_lifecycleLog->setText(tr("crankl_cran_read()\n"
                               "metrics · metadata · slots copied into owned structures\n"
                               "crankl_cran_close()\n"
                               "completed %1 (read → copy → close, one call)")
                                .arg(completedAt));

    m_stack->setCurrentWidget(m_selectedPanel);
}

void HomePage::showUnsupportedFile(const QString &fileName) {
    m_unsupportedFileName->setText(fileName);
    m_stack->setCurrentWidget(m_unsupportedPanel);
}

void HomePage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    // Drives the overview header's compact mode from this page's honest
    // width (see ArchiveHealthHeader::setCompact).
    if (m_overviewHeader)
        m_overviewHeader->setCompact(width() < 940);
}

void HomePage::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void HomePage::dropEvent(QDropEvent *event) {
    const auto urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;
    const QString path = urls.first().toLocalFile();
    if (path.isEmpty())
        return;
    event->acceptProposedAction();
    Q_EMIT pathDropped(path);
}

} // namespace crankl_gui
