#ifndef CRANKL_GUI_CORE_CRANKL_JOB_H
#define CRANKL_GUI_CORE_CRANKL_JOB_H

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QUuid>

namespace crankl_gui {

// The kind of work a CranklJob performs. Every CLI-backed type is executed as
// a `crankl <command> ...` subprocess; OpenArchive and Compare are served
// in-process from the C API on the worker thread.
enum class JobType {
    OpenArchive,
    Pack,
    Turn,
    Finetune,
    Peel,
    Compare,
    Inspect,
};

enum class JobState { Queued, Running, Done, Failed, Cancelled };

// Subprocess stdout/stderr are captured only up to this many bytes per stream
// so a chatty or looping CLI can never grow a job's memory without bound.
inline constexpr int kJobCaptureLimitBytes = 64 * 1024;

// One immutable-parameter snapshot of an operation, plus its live execution
// state. `args` holds everything that follows the `crankl <command>` token for
// subprocess jobs; the executable itself is resolved by JobManager, never
// baked into the job.
struct CranklJob {
    QUuid id = QUuid::createUuid();
    JobType type = JobType::OpenArchive;
    QString operationLabel; // e.g. "turn · demo.crank", shown verbatim in the table
    QString targetPath;     // primary input (archive/weights), shown in the table
    QStringList args;       // CLI arguments after <command>, for subprocess jobs
    QString workingDirectory;
    JobState state = JobState::Queued;
    int progressNumerator = 0;
    int progressDenominator = 0;
    QDateTime startedAt;
    QDateTime finishedAt;
    int exitCode = 0;
    QString stdoutTail;    // last kJobCaptureLimitBytes of stdout
    QString stderrTail;    // last kJobCaptureLimitBytes of stderr
    QString resultSummary; // human-readable completion summary (parsed from output)
    QString errorMessage;  // populated only when state == Failed
};

} // namespace crankl_gui

#endif // CRANKL_GUI_CORE_CRANKL_JOB_H