#include "core/JobManager.h"

#include "core/CliCommands.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonObject>
#include <QMetaObject>
#include <QProcess>
#include <QSettings>

#include <algorithm>
#include <utility>

namespace crankl_gui {

namespace {

// Lives on JobManager's worker thread. Calls ArchiveAdapter (which owns the
// C API mutex) off the GUI thread and reports back via queued signals. Never
// touches m_jobs or any widget.
class ArchiveWorker : public QObject {
    Q_OBJECT
  public:
    using QObject::QObject;

    void doOpen(const QUuid &jobId, const QString &path) {
        Q_EMIT openResult(jobId, ArchiveAdapter::openArchive(path));
    }
    void doCompare(const QUuid &jobId, const QString &pathA, const QString &pathB) {
        Q_EMIT compareResult(jobId, ArchiveAdapter::compareArchives(pathA, pathB));
    }

  Q_SIGNALS:
    void openResult(QUuid jobId, ArchiveOpenResult result);
    void compareResult(QUuid jobId, CompareResult result);
};

// Bounded append for a subprocess output stream.
void appendTail(QString &tail, const QString &text) {
    if (text.isEmpty())
        return;
    tail += text;
    if (tail.size() > kJobCaptureLimitBytes) {
        tail = tail.right(kJobCaptureLimitBytes);
        tail.prepend(QStringLiteral("… (earlier output truncated)\n"));
    }
}

} // namespace

// Executes at most one `crankl` subprocess at a time. Start/cancel are
// invoked queued onto the worker thread; the QProcess lives and runs there.
// Defined at crankl_gui scope (not in the anonymous namespace above) so the
// forward declaration in JobManager.h stays consistent with the moc.
class CliRunner : public QObject {
    Q_OBJECT
  public:
    using QObject::QObject;

    void startJob(const QUuid &jobId, const QString &program, const QStringList &args,
                  const QString &workingDir) {
        if (m_process) {
            if (m_process->state() != QProcess::NotRunning)
                m_process->kill();
            m_process->deleteLater();
            m_process = nullptr;
        }

        auto *process = new QProcess(this);
        process->setProgram(program);
        process->setArguments(args);
        if (!workingDir.isEmpty())
            process->setWorkingDirectory(workingDir);

        m_process = process;
        m_jobId = jobId;
        m_finished = false;

        connect(process, &QProcess::readyReadStandardOutput, this, [this, process] {
            Q_EMIT stdoutChunk(m_jobId, QString::fromUtf8(process->readAllStandardOutput()));
        });
        connect(process, &QProcess::readyReadStandardError, this, [this, process] {
            Q_EMIT stderrChunk(m_jobId, QString::fromUtf8(process->readAllStandardError()));
        });
        connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart && !m_finished) {
                m_finished = true;
                Q_EMIT processFinished(m_jobId, -1, true, process->errorString());
            }
        });
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this, process](int exitCode, QProcess::ExitStatus status) {
                    if (m_finished)
                        return;
                    m_finished = true;
                    const bool crashed = status == QProcess::CrashExit;
                    Q_EMIT processFinished(m_jobId, exitCode, crashed,
                                           crashed ? process->errorString() : QString());
                });

        process->start();
    }

    void cancelJob() {
        if (m_process)
            m_process->kill();
    }

  Q_SIGNALS:
    void stdoutChunk(QUuid jobId, QString text);
    void stderrChunk(QUuid jobId, QString text);
    void processFinished(QUuid jobId, int exitCode, bool crashed, QString errorText);

  private:
    QProcess *m_process = nullptr;
    QUuid m_jobId;
    bool m_finished = true;
};

JobManager::JobManager(QObject *parent) : QObject(parent) {
    qRegisterMetaType<ArchiveOpenResult>("crankl_gui::ArchiveOpenResult");
    qRegisterMetaType<CompareResult>("crankl_gui::CompareResult");

    m_cliPath = defaultCliPath();

    auto *worker = new ArchiveWorker;
    worker->moveToThread(&m_workerThread);
    m_worker = worker;

    auto *runner = new CliRunner;
    runner->moveToThread(&m_workerThread);
    m_cliRunner = runner;

    connect(&m_workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(&m_workerThread, &QThread::finished, runner, &QObject::deleteLater);
    connect(worker, &ArchiveWorker::openResult, this, &JobManager::handleWorkerResult,
            Qt::QueuedConnection);
    connect(worker, &ArchiveWorker::compareResult, this, &JobManager::handleCompareResult,
            Qt::QueuedConnection);
    connect(runner, &CliRunner::stdoutChunk, this, &JobManager::handleCliStdout,
            Qt::QueuedConnection);
    connect(runner, &CliRunner::stderrChunk, this, &JobManager::handleCliStderr,
            Qt::QueuedConnection);
    connect(runner, &CliRunner::processFinished, this, &JobManager::handleCliFinished,
            Qt::QueuedConnection);

    m_workerThread.start();
}

JobManager::~JobManager() {
    m_workerThread.quit();
    m_workerThread.wait();
}

QString JobManager::defaultCliPath() {
    const QByteArray env = qgetenv("CRANKL_CLI");
    if (!env.isEmpty())
        return QString::fromUtf8(env);

    const QString settingsPath = QSettings().value(QStringLiteral("cli/path")).toString();
    if (!settingsPath.isEmpty())
        return settingsPath;

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/crankl"),
        appDir + QStringLiteral("/../crankl"), // GUI and CLI share a build dir
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return QFileInfo(candidate).absoluteFilePath();
    }

    return QStringLiteral("crankl"); // last resort: resolve on PATH
}

void JobManager::setCliPath(const QString &path) {
    m_cliPath = path.isEmpty() ? defaultCliPath() : path;
}

QString JobManager::cliPath() const {
    return m_cliPath;
}

QUuid JobManager::openArchive(const QString &path) {
    CranklJob job;
    job.type = JobType::OpenArchive;
    job.operationLabel = QStringLiteral("cran_read · %1").arg(path.section('/', -1));
    job.targetPath = path;
    job.state = JobState::Running;
    job.startedAt = QDateTime::currentDateTime();

    const QUuid jobId = job.id;
    m_jobs.push_back(job);
    Q_EMIT jobsChanged();

    auto *worker = static_cast<ArchiveWorker *>(m_worker);
    QMetaObject::invokeMethod(worker, [worker, jobId, path]() { worker->doOpen(jobId, path); },
                              Qt::QueuedConnection);
    return jobId;
}

QUuid JobManager::compareArchives(const QString &pathA, const QString &pathB) {
    CranklJob job;
    job.type = JobType::Compare;
    job.operationLabel =
        QStringLiteral("compare · %1 vs %2").arg(pathA.section('/', -1), pathB.section('/', -1));
    job.targetPath = pathA;
    job.state = JobState::Running;
    job.startedAt = QDateTime::currentDateTime();

    const QUuid jobId = job.id;
    m_jobs.push_back(job);
    Q_EMIT jobsChanged();

    auto *worker = static_cast<ArchiveWorker *>(m_worker);
    QMetaObject::invokeMethod(worker,
                              [worker, jobId, a = pathA, b = pathB]() { worker->doCompare(jobId, a, b); },
                              Qt::QueuedConnection);
    return jobId;
}

QUuid JobManager::runCliJob(JobType type, const QString &operationLabel, const QString &targetPath,
                            const QStringList &args, const QString &workingDirectory) {
    CranklJob job;
    job.type = type;
    job.operationLabel = operationLabel;
    job.targetPath = targetPath;
    job.args = args;
    job.workingDirectory = workingDirectory;
    job.state = JobState::Queued;

    const QUuid jobId = job.id;
    m_jobs.push_back(job);
    Q_EMIT jobsChanged();
    pumpQueue();
    return jobId;
}

void JobManager::pumpQueue() {
    if (!m_runningCliJob.isNull())
        return;

    for (const auto &job : std::as_const(m_jobs)) {
        if (job.state != JobState::Queued)
            continue;
        const QUuid jobId = job.id;
        m_runningCliJob = jobId;
        updateJob(jobId, [](CranklJob &j) {
            j.state = JobState::Running;
            j.startedAt = QDateTime::currentDateTime();
        });
        invokeCliStart(jobId, cliCommandName(job.type), job.args, job.workingDirectory);
        return;
    }
}

void JobManager::invokeCliStart(const QUuid &jobId, const QString &command,
                                const QStringList &args, const QString &workingDir) {
    CliRunner *runner = m_cliRunner;
    const QString program = m_cliPath;
    QMetaObject::invokeMethod(
        runner,
        [runner, jobId, program, command, args, workingDir]() {
            QStringList fullArgs{command};
            fullArgs += args;
            runner->startJob(jobId, program, fullArgs, workingDir);
        },
        Qt::QueuedConnection);
}

void JobManager::cancel(const QUuid &jobId) {
    updateJob(jobId, [this, jobId](CranklJob &job) {
        if (job.state == JobState::Queued || job.state == JobState::Running) {
            job.state = JobState::Cancelled;
            job.finishedAt = QDateTime::currentDateTime();
            if (jobId == m_runningCliJob) {
                CliRunner *runner = m_cliRunner;
                QMetaObject::invokeMethod(runner, [runner]() { runner->cancelJob(); },
                                          Qt::QueuedConnection);
            }
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

QVector<CranklJob> JobManager::jobs() const {
    return m_jobs;
}

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

void JobManager::handleCompareResult(QUuid jobId, CompareResult result) {
    updateJob(jobId, [&result](CranklJob &job) {
        if (job.state == JobState::Cancelled)
            return;
        job.state = result.ok ? JobState::Done : JobState::Failed;
        job.finishedAt = QDateTime::currentDateTime();
        job.progressNumerator = job.progressDenominator = 1;
        if (!result.ok) {
            job.errorMessage = result.errorMessage;
        } else {
            job.resultSummary = QStringLiteral("slots changed %1/%2 · hamming %3 · clifford %4")
                                    .arg(result.slotsChanged)
                                    .arg(result.slotsCompared)
                                    .arg(result.hamming, 0, 'f', 4)
                                    .arg(result.cliffordResonance, 0, 'f', 4);
        }
    });
    Q_EMIT compareDone(jobId, result);
}

void JobManager::handleCliStdout(QUuid jobId, QString text) {
    updateJob(jobId, [&text](CranklJob &job) { appendTail(job.stdoutTail, text); });
}

void JobManager::handleCliStderr(QUuid jobId, QString text) {
    updateJob(jobId, [&text](CranklJob &job) { appendTail(job.stderrTail, text); });
}

void JobManager::handleCliFinished(QUuid jobId, int exitCode, bool crashed, QString errorText) {
    updateJob(jobId, [&](CranklJob &job) {
        job.exitCode = exitCode;
        if (job.state == JobState::Cancelled) {
            job.finishedAt = QDateTime::currentDateTime();
            return;
        }
        job.finishedAt = QDateTime::currentDateTime();
        if (crashed || exitCode != 0) {
            job.state = JobState::Failed;
            job.errorMessage = crashed && errorText.isEmpty()
                                   ? QObject::tr("the crankl process crashed (exit code %1)")
                                         .arg(exitCode)
                                   : errorText;
        } else {
            job.state = JobState::Done;
            job.resultSummary = summarizeCliOutput(job);
        }
    });

    if (m_runningCliJob == jobId)
        m_runningCliJob = QUuid();

    // Recompute whether the finished job succeeded so callers get one stable
    // answer even when the job was cancelled or cleared mid-teardown.
    bool ok = false;
    for (const auto &job : std::as_const(m_jobs)) {
        if (job.id == jobId && job.state == JobState::Done) {
            ok = true;
            break;
        }
    }
    Q_EMIT jobFinished(jobId, ok, QString());

    pumpQueue();
}

QString JobManager::summarizeCliOutput(const CranklJob &job) {
    const std::optional<QJsonObject> json = extractJsonFromOutput(job.stdoutTail);

    switch (job.type) {
    case JobType::Compare:
        if (json) {
            const QJsonObject obj = *json;
            return QObject::tr("slots changed %1/%2 · hamming %3 · clifford %4 · sheaf %5")
                .arg(obj.value(QStringLiteral("slots_changed")).toVariant().toString(),
                     obj.value(QStringLiteral("slots_compared")).toVariant().toString(),
                     QString::number(obj.value(QStringLiteral("hamming")).toDouble(), 'f', 4),
                     QString::number(obj.value(QStringLiteral("clifford_resonance")).toDouble(), 'f', 4),
                     QString::number(obj.value(QStringLiteral("sheaf_resonance")).toDouble(), 'f', 4));
        }
        break;
    case JobType::Inspect:
        if (json) {
            const QJsonObject metrics =
                json->value(QStringLiteral("metrics")).toObject();
            return QObject::tr("verify ok · n_slots %1 · depth %2–%3 · trit density %4")
                .arg(metrics.value(QStringLiteral("n_slots")).toVariant().toString(),
                     metrics.value(QStringLiteral("depth_min")).toVariant().toString(),
                     metrics.value(QStringLiteral("depth_max")).toVariant().toString(),
                     QString::number(metrics.value(QStringLiteral("trit_density")).toDouble(), 'f', 4));
        }
        break;
    case JobType::Finetune:
        if (json) {
            return QObject::tr("reconstruction loss %1 → %2")
                .arg(QString::number(json->value(QStringLiteral("recon_before")).toDouble(), 'g', 6),
                     QString::number(json->value(QStringLiteral("recon_after")).toDouble(), 'g', 6));
        }
        break;
    case JobType::Pack:
    case JobType::Turn:
    case JobType::Peel:
        break;
    case JobType::OpenArchive:
        return QString();
    }

    // No structured output: fall back to the tail of plain stdout.
    const QString trimmed = job.stdoutTail.trimmed();
    if (trimmed.isEmpty())
        return job.stderrTail.trimmed().right(200);
    return trimmed.right(200);
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