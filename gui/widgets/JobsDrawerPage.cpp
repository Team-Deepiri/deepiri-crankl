#include "widgets/JobsDrawerPage.h"

#include <QColor>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace crankl_gui {

namespace {

QString stateText(JobState state) {
    switch (state) {
    case JobState::Queued:
        return QObject::tr("queued");
    case JobState::Running:
        return QObject::tr("running");
    case JobState::Done:
        return QObject::tr("done");
    case JobState::Failed:
        return QObject::tr("failed");
    case JobState::Cancelled:
        return QObject::tr("cancelled");
    }
    return {};
}

QColor stateDotColor(JobState state) {
    switch (state) {
    case JobState::Failed:
        return QColor(200, 90, 70);
    case JobState::Running:
    case JobState::Queued:
        return QColor(90, 170, 200);
    case JobState::Cancelled:
        return QColor(110, 110, 110);
    case JobState::Done:
        return QColor(100, 180, 130);
    }
    return QColor(110, 110, 110);
}

QString elapsedText(const CranklJob &job) {
    if (!job.startedAt.isValid())
        return QString();
    const QDateTime end = job.finishedAt.isValid() ? job.finishedAt : QDateTime::currentDateTime();
    const qint64 ms = job.startedAt.msecsTo(end);
    return QStringLiteral("%1 s").arg(ms / 1000.0, 0, 'f', 1);
}

} // namespace

JobsDrawerPage::JobsDrawerPage(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("JobsDrawerPage"));

    auto *layout = new QVBoxLayout(this);

    auto *heading = new QLabel(tr("Jobs"), this);
    heading->setObjectName(QStringLiteral("PageHeading"));
    layout->addWidget(heading);

    auto *subheading = new QLabel(tr("worker QObjects on QThread · queued signals only"), this);
    subheading->setObjectName(QStringLiteral("MutedNote"));
    layout->addWidget(subheading);

    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels(
        {QString(), tr("OPERATION · TARGET"), tr("STATE"), tr("PROGRESS"), tr("ELAPSED")});
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QTableWidget::NoEditTriggers);
    m_table->setSelectionBehavior(QTableWidget::SelectRows);
    layout->addWidget(m_table, 1);

    auto *buttonsRow = new QWidget(this);
    auto *buttonsLayout = new QHBoxLayout(buttonsRow);
    auto *cancelButton = new QPushButton(tr("Cancel running"), buttonsRow);
    connect(cancelButton, &QPushButton::clicked, this, &JobsDrawerPage::cancelRunningRequested);
    auto *clearButton = new QPushButton(tr("Clear finished"), buttonsRow);
    connect(clearButton, &QPushButton::clicked, this, &JobsDrawerPage::clearFinishedRequested);
    auto *note = new QLabel(
        tr("every job carries a UUID, paths, a parameter snapshot, and timestamps"), buttonsRow);
    note->setObjectName(QStringLiteral("MutedNote"));
    buttonsLayout->addWidget(cancelButton);
    buttonsLayout->addWidget(clearButton);
    buttonsLayout->addWidget(note, 1);
    layout->addWidget(buttonsRow);

    m_detail = new QLabel(this);
    m_detail->setObjectName(QStringLiteral("JobErrorDetail"));
    m_detail->setWordWrap(true);
    layout->addWidget(m_detail);
}

void JobsDrawerPage::setJobs(const QVector<CranklJob> &jobs) {
    m_jobs = jobs;
    refreshTable();
}

void JobsDrawerPage::refreshTable() {
    m_table->setRowCount(m_jobs.size());
    QString lastError;

    for (int row = 0; row < m_jobs.size(); ++row) {
        const CranklJob &job = m_jobs[row];

        auto *dot = new QTableWidgetItem;
        dot->setBackground(stateDotColor(job.state));
        m_table->setItem(row, 0, dot);

        m_table->setItem(row, 1, new QTableWidgetItem(job.operationLabel));
        m_table->setItem(row, 2, new QTableWidgetItem(stateText(job.state)));

        const QString progress = job.progressDenominator > 0
                                      ? QStringLiteral("%1/%2")
                                            .arg(job.progressNumerator)
                                            .arg(job.progressDenominator)
                                      : QString();
        m_table->setItem(row, 3, new QTableWidgetItem(progress));
        m_table->setItem(row, 4, new QTableWidgetItem(elapsedText(job)));

        if (job.state == JobState::Failed && !job.errorMessage.isEmpty())
            lastError = tr("%1: %2").arg(job.operationLabel, job.errorMessage);
    }

    m_detail->setText(lastError);
}

} // namespace crankl_gui
