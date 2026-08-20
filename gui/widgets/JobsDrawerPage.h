#ifndef CRANKL_GUI_WIDGETS_JOBS_DRAWER_PAGE_H
#define CRANKL_GUI_WIDGETS_JOBS_DRAWER_PAGE_H

#include "core/CranklJob.h"

#include <QVector>
#include <QWidget>

class QTableWidget;
class QLabel;
class QPlainTextEdit;
class QTabWidget;

namespace crankl_gui {

// §16: the Jobs drawer. A table of every CranklJob -- status dot,
// operation·target, state, exit code, progress, elapsed -- fed by JobManager,
// plus Cancel running / Clear finished actions. Selecting a row opens a
// stdout / stderr / error detail view beneath it, fed by the bounded capture
// tails stored on the job.
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
    void updateDetail(int row);

    QTableWidget *m_table = nullptr;
    QLabel *m_detail = nullptr;
    QPlainTextEdit *m_stdoutView = nullptr;
    QPlainTextEdit *m_stderrView = nullptr;
    QTabWidget *m_outputTabs = nullptr;
    QVector<CranklJob> m_jobs;
};

} // namespace crankl_gui

#endif // CRANKL_GUI_WIDGETS_JOBS_DRAWER_PAGE_H