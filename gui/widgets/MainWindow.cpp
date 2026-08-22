#include "widgets/MainWindow.h"

#include "core/JobManager.h"
#include "widgets/ComparePage.h"
#include "widgets/HomePage.h"
#include "widgets/InspectPage.h"
#include "widgets/JobsDrawerPage.h"
#include "widgets/NewJobDialog.h"
#include "widgets/PlaceholderPage.h"

#include <QCursor>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QStackedWidget>
#include <QToolBar>
#include <QToolButton>

#include <crankl/version_api.h>

#include <utility>

namespace crankl_gui {

namespace {

// Destinations without a Phase 2 screen. Pack/Optimize/History/Bind/Forward/
// Unpack/Pipeline and the Math Lab stay dimmed placeholders; Compare now has
// a real page (see buildCentralArea).
const QVector<NavDestination> kPlaceholderDestinations = {
    NavDestination::Pack,     NavDestination::Optimize,        NavDestination::History,
    NavDestination::Bind,     NavDestination::Forward,         NavDestination::Unpack,
    NavDestination::Pipeline, NavDestination::AdvancedMathLab,
};

QString placeholderTitle(NavDestination d) {
    switch (d) {
    case NavDestination::Compare:
        return QObject::tr("Compare");
    case NavDestination::Pack:
        return QObject::tr("Pack");
    case NavDestination::Optimize:
        return QObject::tr("Optimize");
    case NavDestination::History:
        return QObject::tr("History");
    case NavDestination::Bind:
        return QObject::tr("Bind");
    case NavDestination::Forward:
        return QObject::tr("Forward");
    case NavDestination::Unpack:
        return QObject::tr("Unpack");
    case NavDestination::Pipeline:
        return QObject::tr("Pipeline");
    case NavDestination::AdvancedMathLab:
        return QObject::tr("Advanced Math Lab");
    case NavDestination::Settings:
        return QObject::tr("Settings");
    case NavDestination::Help:
        return QObject::tr("Help");
    default:
        return {};
    }
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    const QString version = QString::fromUtf8(crankl_version_string());
    setWindowTitle(tr("crankl %1").arg(version));
    resize(1360, 860);
    // Freely resizable, with a floor that fits the smallest common desktop
    // displays (1280x720 / 1366x768 / 1024x768, minus OS chrome). Below
    // ~1100px of page width the three-column pages reflow (right column
    // drops under the center), and pages scroll vertically on short
    // screens, so nothing inside ever forces the window wider or taller.
    setMinimumSize(1024, 600);

    m_jobManager = new JobManager(this);
    connect(m_jobManager, &JobManager::archiveOpened, this, &MainWindow::handleArchiveOpened);
    connect(m_jobManager, &JobManager::jobsChanged, this, &MainWindow::handleJobsChanged);

    buildToolBar();
    buildCentralArea();

    connect(m_nav, &NavigationList::destinationActivated, this,
            &MainWindow::handleDestinationActivated);

    handleDestinationActivated(m_nav->currentDestination());
    updateBreadcrumb();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildToolBar() {
    auto *toolBar = addToolBar(tr("Main"));
    toolBar->setMovable(false);

    auto *openButton = new QToolButton(this);
    openButton->setText(tr("Open ▾"));
    openButton->setPopupMode(QToolButton::InstantPopup);
    auto *openMenu = new QMenu(openButton);
    openMenu->addAction(tr("Open archive…"), this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open crank archive"), QString(),
                                                          tr("Crank archives (*.crank)"));
        if (!path.isEmpty())
            openPath(path);
    });
    openMenu->addAction(tr("Open weights…"), this, [this] {
        const QString path =
            QFileDialog::getOpenFileName(this, tr("Open weight file"), QString(),
                                         tr("Weight files (*.f32 *.bin *.safetensors)"));
        if (!path.isEmpty())
            openPath(path);
    });
    openButton->setMenu(openMenu);
    toolBar->addWidget(openButton);

    auto *newOperationButton = new QToolButton(this);
    newOperationButton->setText(tr("New operation"));
    connect(newOperationButton, &QToolButton::clicked, this, [this] { showNewJobDialog(); });
    toolBar->addWidget(newOperationButton);

    toolBar->addSeparator();

    m_breadcrumbArchive = new QLabel(this);
    m_breadcrumbArchive->setObjectName(QStringLiteral("Breadcrumb"));
    toolBar->addWidget(m_breadcrumbArchive);

    auto *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);

    m_jobsIndicator = new QLabel(tr("jobs idle"), this);
    m_jobsIndicator->setObjectName(QStringLiteral("JobsIndicator"));
    toolBar->addWidget(m_jobsIndicator);

    auto *helpButton = new QToolButton(this);
    helpButton->setText(QStringLiteral("?"));
    connect(helpButton, &QToolButton::clicked, this,
            [this] { m_nav->setCurrentDestination(NavDestination::Help); });
    toolBar->addWidget(helpButton);
}

void MainWindow::buildCentralArea() {
    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_nav = new NavigationList(central);
    m_nav->setFixedWidth(212);
    layout->addWidget(m_nav);

    m_stack = new QStackedWidget(central);
    layout->addWidget(m_stack, 1);

    setCentralWidget(central);

    m_homePage = new HomePage(m_stack);
    connect(m_homePage, &HomePage::openArchiveRequested, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open crank archive"), QString(),
                                                          tr("Crank archives (*.crank)"));
        if (!path.isEmpty())
            openPath(path);
    });
    connect(m_homePage, &HomePage::openWeightsRequested, this, [this] {
        const QString path =
            QFileDialog::getOpenFileName(this, tr("Open weight file"), QString(),
                                         tr("Weight files (*.f32 *.bin *.safetensors)"));
        if (!path.isEmpty())
            openPath(path);
    });
    connect(m_homePage, &HomePage::pathDropped, this, &MainWindow::openPath);
    connect(m_homePage, &HomePage::inspectRequested, this,
            [this] { m_nav->setCurrentDestination(NavDestination::Inspect); });
    connect(m_homePage, &HomePage::compareRequested, this, [this] {
        if (m_currentArchive)
            m_comparePage->setArchiveA(m_currentArchive->path);
        m_nav->setCurrentDestination(NavDestination::Compare);
    });
    connect(m_homePage, &HomePage::newJobRequested, this, &MainWindow::showNewJobDialog);
    connect(m_homePage, &HomePage::reVerifyRequested, this, &MainWindow::handleVerifyRequested);
    connect(m_homePage, &HomePage::closeArchiveRequested, this, &MainWindow::handleCloseRequested);

    m_inspectPage = new InspectPage(m_stack);
    connect(m_inspectPage, &InspectPage::verifyRequested, this, &MainWindow::handleVerifyRequested);
    connect(m_inspectPage, &InspectPage::closeRequested, this, &MainWindow::handleCloseRequested);
    connect(m_inspectPage, &InspectPage::changeArchiveRequested, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open crank archive"), QString(),
                                                          tr("Crank archives (*.crank)"));
        if (!path.isEmpty())
            openPath(path);
    });
    connect(m_inspectPage, &InspectPage::pathDropped, this, &MainWindow::openPath);
    connect(m_inspectPage, &InspectPage::recentRequested, this, &MainWindow::showRecentMenu);

    m_comparePage = new ComparePage(m_stack);
    connect(m_comparePage, &ComparePage::compareRequested, m_jobManager,
            &JobManager::compareArchives);
    connect(m_jobManager, &JobManager::compareDone, this, &MainWindow::handleCompareDone);

    m_jobsPage = new JobsDrawerPage(m_stack);
    connect(m_jobsPage, &JobsDrawerPage::clearFinishedRequested, m_jobManager,
            &JobManager::clearFinished);
    connect(m_jobsPage, &JobsDrawerPage::cancelRunningRequested, m_jobManager,
            &JobManager::cancelRunning);

    m_pages.insert(NavDestination::Home, m_homePage);
    m_pages.insert(NavDestination::Inspect, m_inspectPage);
    m_pages.insert(NavDestination::Compare, m_comparePage);
    m_pages.insert(NavDestination::Jobs, m_jobsPage);

    for (NavDestination destination : kPlaceholderDestinations) {
        auto *page = new PlaceholderPage(placeholderTitle(destination), m_stack);
        m_pages.insert(destination, page);
    }
    // Settings and Help are enabled nav rows, but the imported design has no
    // drawn screen for either -- distinguish that from the dimmed-by-design
    // placeholders above rather than reusing the same "Not available yet."
    // wording, which would misrepresent them as disabled.
    m_pages.insert(NavDestination::Settings,
                   new PlaceholderPage(placeholderTitle(NavDestination::Settings), m_stack));
    m_pages.insert(NavDestination::Help,
                   new PlaceholderPage(placeholderTitle(NavDestination::Help), m_stack));

    for (auto *page : std::as_const(m_pages))
        m_stack->addWidget(page);
}

void MainWindow::handleDestinationActivated(NavDestination destination) {
    if (auto *page = m_pages.value(destination))
        m_stack->setCurrentWidget(page);
}

QWidget *MainWindow::pageForDestination(NavDestination destination) {
    return m_pages.value(destination);
}

void MainWindow::openPath(const QString &path) {
    if (!path.endsWith(QStringLiteral(".crank"), Qt::CaseInsensitive)) {
        m_homePage->showUnsupportedFile(QFileInfo(path).fileName());
        m_nav->setCurrentDestination(NavDestination::Home);
        return;
    }
    m_pendingArchivePath = path;
    m_jobManager->openArchive(path);
}

void MainWindow::handleVerifyRequested() {
    if (!m_currentArchive)
        return;
    m_pendingArchivePath = m_currentArchive->path;
    m_jobManager->openArchive(m_currentArchive->path);
}

void MainWindow::handleCloseRequested() {
    m_currentArchive.reset();
    m_pendingArchivePath.clear();
    m_homePage->showEmptyState();
    m_inspectPage->clearSnapshot();
    updateBreadcrumb();
    m_nav->setCurrentDestination(NavDestination::Home);
}

void MainWindow::handleArchiveOpened(QUuid /*jobId*/, ArchiveOpenResult result) {
    if (!result.ok) {
        showOpenFailure(result.errorMessage);
        return;
    }
    m_currentArchive = result.snapshot;
    m_pendingArchivePath.clear();
    m_homePage->showArchive(*m_currentArchive);
    m_inspectPage->setSnapshot(*m_currentArchive);
    rememberRecent(m_currentArchive->path);
    updateBreadcrumb();
}

void MainWindow::handleCompareDone(QUuid /*jobId*/, CompareResult result) {
    m_comparePage->showResult(result);
}

void MainWindow::showNewJobDialog() {
    auto *dialog = new NewJobDialog(this);
    if (m_currentArchive)
        dialog->setInputPath(m_currentArchive->path);
    connect(dialog, &NewJobDialog::jobRequested, m_jobManager, &JobManager::runCliJob);
    connect(dialog, &NewJobDialog::finished, dialog, &QObject::deleteLater);
    dialog->open();
}

void MainWindow::debugDumpWidths() {
    qDebug() << "window" << width() << "stack" << m_stack->width() << "inspect"
             << m_inspectPage->width() << "inspectMin" << m_inspectPage->minimumSizeHint().width();
    m_inspectPage->debugDump();
}

void MainWindow::rememberRecent(const QString &path) {
    QSettings settings;
    QStringList recents = settings.value(QStringLiteral("recent/archives")).toStringList();
    recents.removeAll(path);
    recents.prepend(path);
    while (recents.size() > 8)
        recents.removeLast();
    settings.setValue(QStringLiteral("recent/archives"), recents);
}

void MainWindow::showRecentMenu() {
    const QStringList recents = QSettings().value(QStringLiteral("recent/archives")).toStringList();
    QMenu menu(this);
    if (recents.isEmpty()) {
        menu.addAction(tr("No recent archives"))->setEnabled(false);
    } else {
        for (const QString &path : recents) {
            menu.addAction(path, this, [this, path] { openPath(path); });
        }
    }
    menu.exec(QCursor::pos());
}

void MainWindow::handleJobsChanged() {
    const QVector<CranklJob> jobs = m_jobManager->jobs();
    m_jobsPage->setJobs(jobs);

    int running = 0;
    for (const auto &job : jobs) {
        if (job.state == JobState::Running || job.state == JobState::Queued)
            ++running;
    }
    m_nav->setJobsCount(jobs.size());
    m_jobsIndicator->setText(running > 0 ? tr("jobs running (%1)").arg(running) : tr("jobs idle"));
}

void MainWindow::showOpenFailure(const QString &message) {
    QMessageBox::warning(this, tr("Could not open archive"), message);
}

void MainWindow::updateBreadcrumb() {
    if (!m_breadcrumbArchive)
        return;
    const QString archivePart =
        m_currentArchive ? m_currentArchive->fileName : tr("no archive open");
    m_breadcrumbArchive->setText(tr("no workspace  ›  %1").arg(archivePart));
}

} // namespace crankl_gui
