#include "core/JobManager.h"

#include <QDateTime>
#include <QMetaObject>

#include <algorithm>
#include <utility>

namespace crankl_gui {

namespace {

// Lives on JobManager's worker thread. Its only job is to call
// ArchiveAdapter::openArchive() off the GUI thread and report the result
// back via a queued signal -- it never touches m_jobs or any widget.
class ArchiveOpenWorker : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

    void doOpen(const QUuid &jobId, const QString &path) {
        ArchiveOpenResult result = ArchiveAdapter::openArchive(path);
        Q_EMIT resultReady(jobId, result);
    }

Q_SIGNALS:
    void resultReady(QUuid jobId, ArchiveOpenResult result);
};

} // namespace

JobManager::JobManager(QObject *parent) : QObject(parent) {
    qRegisterMetaType<ArchiveOpenResult>("crankl_gui::ArchiveOpenResult");

    auto *worker = new ArchiveOpenWorker;
    worker->moveToThread(&m_workerThread);
    m_worker = worker;

    connect(&m_workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &ArchiveOpenWorker::resultReady, this, &JobManager::handleWorkerResult,
            Qt::QueuedConnection);

    m_workerThread.start();
}

JobManager::~JobManager() {
    m_workerThread.quit();
    m_workerThread.wait();
}

QUuid JobManager::openArchive(const QString &path) {
    CranklJob job;
    job.operationLabel = QStringLiteral("cran_read · %1").arg(path.section('/', -1));
    job.targetPath = path;
    job.state = JobState::Running;
    job.startedAt = QDateTime::currentDateTime();

    const QUuid jobId = job.id;
    m_jobs.push_back(job);
    Q_EMIT jobsChanged();

    auto *worker = static_cast<ArchiveOpenWorker *>(m_worker);
    QMetaObject::invokeMethod(
        worker, [worker, jobId, path]() { worker->doOpen(jobId, path); }, Qt::QueuedConnection);
    return jobId;
}

void JobManager::cancel(const QUuid &jobId) {
    updateJob(jobId, [](CranklJob &job) {
        if (job.state == JobState::Queued || job.state == JobState::Running) {
            job.state = JobState::Cancelled;
            job.finishedAt = QDateTime::currentDateTime();
        }
    });
}

void JobManager::cancelRunning() {
    for (const auto &job : std::as_const(m_jobs)) {
        if (job.state == JobState::Queued || job.state == JobState::Running)
            cancel(job.id);
    }
}

void JobManager::clearFinished() {
    m_jobs.erase(std::remove_if(m_jobs.begin(), m_jobs.end(),
                                 [](const CranklJob &job) {
                                     return job.state == JobState::Done ||
                                            job.state == JobState::Failed ||
                                            job.state == JobState::Cancelled;
                                 }),
                 m_jobs.end());
    Q_EMIT jobsChanged();
}

QVector<CranklJob> JobManager::jobs() const { return m_jobs; }

void JobManager::handleWorkerResult(QUuid jobId, ArchiveOpenResult result) {
    updateJob(jobId, [&result](CranklJob &job) {
        if (job.state == JobState::Cancelled)
            return;
        job.state = result.ok ? JobState::Done : JobState::Failed;
        job.finishedAt = QDateTime::currentDateTime();
        job.progressNumerator = job.progressDenominator = 1;
        if (!result.ok)
            job.errorMessage = result.errorMessage;
    });
    Q_EMIT archiveOpened(jobId, result);
}

void JobManager::updateJob(const QUuid &jobId, const std::function<void(CranklJob &)> &mutator) {
    for (auto &job : m_jobs) {
        if (job.id == jobId) {
            mutator(job);
            Q_EMIT jobsChanged();
            return;
        }
    }
}

} // namespace crankl_gui

#include "JobManager.moc"
