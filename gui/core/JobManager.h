#ifndef CRANKL_GUI_CORE_JOB_MANAGER_H
#define CRANKL_GUI_CORE_JOB_MANAGER_H

#include "core/ArchiveAdapter.h"
#include "core/CranklJob.h"

#include <QObject>
#include <QThread>
#include <QVector>

#include <functional>

namespace crankl_gui {

class JobManager : public QObject {
    Q_OBJECT
  public:
    explicit JobManager(QObject *parent = nullptr);
    ~JobManager() override;

    // Enqueues an open-archive job and returns its id immediately; the
    // result arrives later via archiveOpened().
    QUuid openArchive(const QString &path);

    // Cooperative only in Phase 1: the backend has no progress-callback
    // hooks yet (see docs/GUI_DESIGN_SPEC.md §16.2), and an open-archive job
    // is short enough that this mainly cancels queued-but-not-started jobs.
    void cancel(const QUuid &jobId);
    void cancelRunning(); // cancels every Queued/Running job
    void clearFinished();

    QVector<CranklJob> jobs() const;

  Q_SIGNALS:
    void jobsChanged();
    void archiveOpened(QUuid jobId, ArchiveOpenResult result);

  private Q_SLOTS:
    void handleWorkerResult(QUuid jobId, ArchiveOpenResult result);

  private:
    void updateJob(const QUuid &jobId, const std::function<void(CranklJob &)> &mutator);

    QThread m_workerThread;
    QObject *m_worker = nullptr; // lives on m_workerThread; see JobManager.cpp
    QVector<CranklJob> m_jobs;
};

} // namespace crankl_gui

#endif // CRANKL_GUI_CORE_JOB_MANAGER_H
