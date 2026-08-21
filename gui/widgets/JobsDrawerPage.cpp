#include "widgets/JobsDrawerPage.h"

#include <QColor>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace crankl_gui {

namespace {

constexpr int kStateColumn = 2;
constexpr int kExitCodeColumn = 3;

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

QString exitCodeText(const CranklJob &job) {
    switch (job.state) {
    case JobState::Done:
        return QObject::tr("0");
    case JobState::Failed:
        return job.exitCode != 0 ? QString::number(job.exitCode) : QObject::tr("≠0");
    case JobState::Cancelled:
        return QObject::tr("—");
    case JobState::Queued:
    case JobState::Running:
        return QString();
    }
    return {};
}

QPlainTextEdit *makeOutputView(QWidget *parent) {
    auto *view = new QPlainTextEdit(parent);
    view->setReadOnly(true);
    view->setObjectName(QStringLiteral("JobOutputView"));
    view->setMinimumHeight(140);
    return view;
}

} // namespace

JobsDrawerPage::JobsDrawerPage(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("JobsDrawerPage"));

    auto *layout = new QVBoxLayout(this);

    auto *heading = new QLabel(tr("Jobs"), this);
    heading->setObjectName(QStringLiteral("PageHeading"));
    layout->addWidget(heading);

    auto *subheading = new QLabel(
        tr("real crankl CLI subprocesses · one at a time · stdout/stderr captured (last 64 KB)"),
        this);
    subheading->setObjectName(QStringLiteral("MutedNote"));
    layout->addWidget(subheading);

    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels({QString(), tr("OPERATION · TARGET"), tr("STATE"),
                                        tr("EXIT"), tr("PROGRESS"), tr("ELAPSED")});
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QTableWidget::NoEditTriggers);
    m_table->setSelectionBehavior(QTableWidget::SelectRows);
    connect(m_table, &QTableWidget::itemSelectionChanged, this,
            [this] { updateDetail(m_table->currentRow()); });
    layout->addWidget(m_table, 3);

    m_outputTabs = new QTabWidget(this);
    m_stdoutView = makeOutputView(m_outputTabs);
    m_stderrView = makeOutputView(m_outputTabs);
    m_outputTabs->addTab(m_stdoutView, tr("stdout"));
    m_outputTabs->addTab(m_stderrView, tr("stderr"));
    m_outputTabs->setVisible(false);
    layout->addWidget(m_outputTabs, 2);

    m_detail = new QLabel(this);
    m_detail->setObjectName(QStringLiteral("JobErrorDetail"));
    m_detail->setWordWrap(true);
    layout->addWidget(m_detail);

    auto *buttonsRow = new QWidget(this);
    auto *buttonsLayout = new QHBoxLayout(buttonsRow);
    auto *cancelButton = new QPushButton(tr("Cancel running"), buttonsRow);
    connect(cancelButton, &QPushButton::clicked, this, &JobsDrawerPage::cancelRunningRequested);
    auto *clearButton = new QPushButton(tr("Clear finished"), buttonsRow);
    connect(clearButton, &QPushButton::clicked, this, &JobsDrawerPage::clearFinishedRequested);
    auto *note =
        new QLabel(tr("queued, running, done, failed, cancelled — cancellation kills the QProcess"),
                   buttonsRow);
    note->setObjectName(QStringLiteral("MutedNote"));
    buttonsLayout->addWidget(cancelButton);
    buttonsLayout->addWidget(clearButton);
    buttonsLayout->addWidget(note, 1);
    layout->addWidget(buttonsRow);
}

void JobsDrawerPage::setJobs(const QVector<CranklJob> &jobs) {
    const int previous = m_table->currentRow();
    m_jobs = jobs;
    refreshTable();
    if (previous >= 0 && previous < m_table->rowCount())
        m_table->setCurrentCell(previous, 0);
    updateDetail(m_table->currentRow());
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
        m_table->setItem(row, kExitCodeColumn, new QTableWidgetItem(exitCodeText(job)));

        const QString progress =
            job.progressDenominator > 0
                ? QStringLiteral("%1/%2").arg(job.progressNumerator).arg(job.progressDenominator)
                : QString();
        m_table->setItem(row, 4, new QTableWidgetItem(progress));
        m_table->setItem(row, 5, new QTableWidgetItem(elapsedText(job)));

        if (job.state == JobState::Failed && !job.errorMessage.isEmpty())
            lastError = tr("%1: %2").arg(job.operationLabel, job.errorMessage);
    }

    m_detail->setText(lastError);
}

void JobsDrawerPage::updateDetail(int row) {
    if (row < 0 || row >= m_jobs.size()) {
        m_outputTabs->setVisible(false);
        m_stdoutView->clear();
        m_stderrView->clear();
        return;
    }
    const CranklJob &job = m_jobs[row];
    m_stdoutView->setPlainText(job.stdoutTail);
    m_stderrView->setPlainText(job.stderrTail);
    m_outputTabs->setVisible(!job.stdoutTail.isEmpty() || !job.stderrTail.isEmpty());
}

} // namespace crankl_gui