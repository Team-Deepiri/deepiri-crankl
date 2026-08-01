#ifndef CRANKL_GUI_WIDGETS_MAIN_WINDOW_H
#define CRANKL_GUI_WIDGETS_MAIN_WINDOW_H

#include "core/ArchiveAdapter.h"
#include "widgets/NavigationList.h"

#include <QHash>
#include <QMainWindow>

#include <optional>

class QLabel;
class QStackedWidget;
class QToolButton;

namespace crankl_gui {

class JobManager;
class HomePage;
class InspectPage;
class JobsDrawerPage;

// The application shell: QMainWindow + QToolBar + NavigationList (left) +
// QStackedWidget (right)
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Dev hooks used by main.cpp's CRANKL_GUI_SCREENSHOT harness only.
    void debugOpenPath(const QString &path) { openPath(path); }
    void debugShowInspect() { m_nav->setCurrentDestination(NavDestination::Inspect); }
    void debugDumpWidths();

private Q_SLOTS:
    void handleDestinationActivated(NavDestination destination);
    void handleArchiveOpened(QUuid jobId, ArchiveOpenResult result);
    void handleJobsChanged();
    void handleVerifyRequested();
    void handleCloseRequested();

private:
    void buildToolBar();
    void buildCentralArea();
    void openPath(const QString &path); // shared by the Open button and any drop target
    void showOpenFailure(const QString &message);
    void updateBreadcrumb();
    void rememberRecent(const QString &path);
    void showRecentMenu();
    QWidget *pageForDestination(NavDestination destination);

    NavigationList *m_nav = nullptr;
    QStackedWidget *m_stack = nullptr;
    JobManager *m_jobManager = nullptr;

    HomePage *m_homePage = nullptr;
    InspectPage *m_inspectPage = nullptr;
    JobsDrawerPage *m_jobsPage = nullptr;
    QHash<NavDestination, QWidget *> m_pages;

    QLabel *m_breadcrumbArchive = nullptr;
    QLabel *m_jobsIndicator = nullptr;

    std::optional<ArchiveSnapshot> m_currentArchive;
    QString m_pendingArchivePath; // path of the job currently in flight, if any
};

} // namespace crankl_gui

#endif // CRANKL_GUI_WIDGETS_MAIN_WINDOW_H
