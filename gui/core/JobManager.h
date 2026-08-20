#ifndef CRANKL_GUI_CORE_JOB_MANAGER_H
#define CRANKL_GUI_CORE_JOB_MANAGER_H

#include "core/ArchiveAdapter.h"
#include "core/CranklJob.h"

#include <QObject>
#include <QThread>
#include <QVector>

#include <functional>

namespace crankl_gui {

class CliRunner;

// Owns every CranklJob in the app. Two execution paths, both on one worker
// thread:
//
//  - In-process C-API jobs (openArchive, compareArchives) run through the
//    ArchiveAdapter on the worker and report results back via queued signals.
//  - Subprocess jobs (pack/turn/finetune/peel/compare/inspect) run the real
//    `crankl` CLI through a QProcess, strictly one at a time (FIFO) so two
//    archives are never touched concurrently, with cancellation by kill and
//    bounded stdout/stderr capture (last 64 KB per stream).
//
// The job vector is the single source of truth and is only ever mutated on
// the GUI thread; the worker is stateless between jobs.
class JobManager : public QObject {
    Q_OBJECT
  public:
    explicit JobManager(QObject *parent = nullptr);
    ~JobManager() override;

    // Resolves the CLI binary: env CRANKL_CLI, then QSettings "cli/path",
    // then a sibling `crankl` next to the app binary, then "crankl" on PATH.
    static QString defaultCliPath();
    void setCliPath(const QString &path);
    QString cliPath() const;

    // In-process C-API jobs; the result arrives later via archiveOpened() /
    // compareDone(). Returns the job id immediately.
    QUuid openArchive(const QString &path);
    QUuid compareArchives(const QString &pathA, const QString &pathB);

    // Subprocess job. `args` holds everything after `crankl <command>`.
    QUuid runCliJob(JobType type, const QString &operationLabel, const QString &targetPath,
                    const QStringList &args, const QString &workingDirectory);

    void cancel(const QUuid &jobId); // queued -> Cancelled; running -> kill
    void cancelRunning();
    void clearFinished();

    QVector<CranklJob> jobs() const;

  Q_SIGNALS:
    void jobsChanged();
    void archiveOpened(QUuid jobId, ArchiveOpenResult result);
    void compareDone(QUuid jobId, CompareResult result);
    void jobFinished(QUuid jobId, bool ok, QString summary);

  private Q_SLOTS:
    void handleWorkerResult(QUuid jobId, ArchiveOpenResult result);
    void handleCompareResult(QUuid jobId, CompareResult result);
    void handleCliStdout(QUuid jobId, QString text);
    void handleCliStderr(QUuid jobId, QString text);
    void handleCliFinished(QUuid jobId, int exitCode, bool crashed, QString errorText);

  private:
    void updateJob(const QUuid &jobId, const std::function<void(CranklJob &)> &mutator);
    void pumpQueue(); // start the next Queued CLI job if one is running none
    void invokeCliStart(const QUuid &jobId, const QString &command, const QStringList &args,
                        const QString &workingDir);
    QString summarizeCliOutput(const CranklJob &job);

    QThread m_workerThread;
    QObject *m_worker = nullptr;      // lives on m_workerThread (open/compare)
    CliRunner *m_cliRunner = nullptr; // lives on m_workerThread (QProcess)
    QVector<CranklJob> m_jobs;
    QString m_cliPath;
    QUuid m_runningCliJob; // subprocess currently executing, if any
};

} // namespace crankl_gui

#endif // CRANKL_GUI_CORE_JOB_MANAGER_H