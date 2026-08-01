#ifndef CRANKL_GUI_WIDGETS_NAVIGATION_LIST_H
#define CRANKL_GUI_WIDGETS_NAVIGATION_LIST_H

#include <QWidget>

class QListWidget;
class QListWidgetItem;

namespace crankl_gui {

enum class NavDestination {
    Home,
    Inspect,
    Compare,
    Pack,
    Optimize,
    History,
    Bind,
    Forward,
    Unpack,
    Pipeline,
    AdvancedMathLab,
    Jobs,
    Settings,
    Help,
};

// Left-hand nav: only Home, Inspect, Jobs, Settings, and Help are
// enabled in Phase 1 -- every other destination is disabled and routes to a
// PlaceholderPage. 
class NavigationList : public QWidget {
    Q_OBJECT
public:
    explicit NavigationList(QWidget *parent = nullptr);

    void setJobsCount(int count);
    NavDestination currentDestination() const;
    void setCurrentDestination(NavDestination destination);

Q_SIGNALS:
    void destinationActivated(NavDestination destination);

private:
    void addGroupLabel(const QString &text);
    void addDestinationRow(NavDestination destination, const QString &label, bool enabled);
    void restorePersistedDestination();

    QListWidget *m_list = nullptr;
    QListWidgetItem *m_jobsItem = nullptr;
};

} // namespace crankl_gui

#endif // CRANKL_GUI_WIDGETS_NAVIGATION_LIST_H
