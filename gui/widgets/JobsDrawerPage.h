#ifndef CRANKL_GUI_WIDGETS_JOBS_DRAWER_PAGE_H
#define CRANKL_GUI_WIDGETS_JOBS_DRAWER_PAGE_H

#include "core/CranklJob.h"

#include <QVector>
#include <QWidget>

class QTableWidget;
class QLabel;

namespace crankl_gui {

// §16: the Jobs drawer. A table of every CranklJob -- status dot,
// operation·target, state, progress, elapsed -- fed by JobManager, plus
// Cancel running / Clear finished actions and a detail strip showing the
// error message of the most recently failed job
class JobsDrawerPage : public QWidget {
    Q_OBJECT
public:
    explicit JobsDrawerPage(QWidget *parent = nullptr);

public Q_SLOTS:
    void setJobs(const QVector<CranklJob> &jobs);

Q_SIGNALS:
    void cancelRunningRequested();
    void clearFinishedRequested();

private:
    void refreshTable();

    QTableWidget *m_table = nullptr;
    QLabel *m_detail = nullptr;
    QVector<CranklJob> m_jobs;
};

} // namespace crankl_gui

#endif // CRANKL_GUI_WIDGETS_JOBS_DRAWER_PAGE_H
