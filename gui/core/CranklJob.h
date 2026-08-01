#ifndef CRANKL_GUI_CORE_CRANKL_JOB_H
#define CRANKL_GUI_CORE_CRANKL_JOB_H

#include <QDateTime>
#include <QString>
#include <QUuid>

namespace crankl_gui {

enum class JobState { Queued, Running, Done, Failed, Cancelled };

struct CranklJob {
    QUuid id = QUuid::createUuid();
    QString operationLabel; // e.g. "verify · demo.crank", shown verbatim in the table
    QString targetPath;
    JobState state = JobState::Queued;
    int progressNumerator = 0;
    int progressDenominator = 0;
    QDateTime startedAt;
    QDateTime finishedAt;
    QString errorMessage; // populated only when state == Failed
};

} // namespace crankl_gui

#endif // CRANKL_GUI_CORE_CRANKL_JOB_H
