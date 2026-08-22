#include "widgets/ComparePage.h"

#include "widgets/StyleUtil.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <iterator>
#include <utility>

namespace crankl_gui {

namespace {

QLabel *makeSummaryValue(const QString &text, QWidget *parent) {
    auto *value = new QLabel(text, parent);
    value->setObjectName(QStringLiteral("CompareSummaryValue"));
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return value;
}

QString deltaText(double delta) {
    return QStringLiteral("%1%2")
        .arg(delta >= 0 ? QStringLiteral("+") : QString())
        .arg(delta, 0, 'f', 4);
}

} // namespace

ComparePage::ComparePage(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("ComparePage"));
    setAcceptDrops(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);

    auto *headingRow = new QWidget(this);
    auto *headingLayout = new QHBoxLayout(headingRow);
    headingLayout->setContentsMargins(0, 0, 0, 0);
    auto *heading = new QLabel(tr("Compare two archives"), headingRow);
    heading->setObjectName(QStringLiteral("PageHeading"));
    auto *tag = new QLabel(tr("diff · hamming · resonance · metric deltas"), headingRow);
    tag->setObjectName(QStringLiteral("ColumnTag"));
    headingLayout->addWidget(heading);
    headingLayout->addStretch(1);
    headingLayout->addWidget(tag);
    layout->addWidget(headingRow);

    // --- Inputs: Archive A / swap / Archive B.
    auto *inputsCard = makeCard(QStringLiteral("GroupCard"), this);
    auto *inputsLayout = new QVBoxLayout(inputsCard);
    inputsLayout->setContentsMargins(14, 13, 14, 13);
    inputsLayout->setSpacing(10);

    auto addPathRow = [&](const QString &labelText, QLineEdit *&lineEdit) {
        auto *row = new QWidget(inputsCard);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);
        auto *label = new QLabel(labelText, row);
        label->setObjectName(QStringLiteral("ParamLabel"));
        label->setFixedWidth(64);
        lineEdit = new QLineEdit(row);
        lineEdit->setObjectName(QStringLiteral("ComparePathEdit"));
        lineEdit->setPlaceholderText(tr("path to a .crank archive"));
        auto *browse = new QPushButton(tr("Browse…"), row);
        browse->setObjectName(QStringLiteral("SecondaryButton"));
        connect(browse, &QPushButton::clicked, this, [this, lineEdit] {
            const QString path = QFileDialog::getOpenFileName(
                this, tr("Open crank archive"), QString(), tr("Crank archives (*.crank)"));
            if (!path.isEmpty())
                lineEdit->setText(path);
        });
        rowLayout->addWidget(label);
        rowLayout->addWidget(lineEdit, 1);
        rowLayout->addWidget(browse);
        inputsLayout->addWidget(row);
    };
    addPathRow(tr("Archive A"), m_pathA);
    addPathRow(tr("Archive B"), m_pathB);

    auto *actionsRow = new QWidget(inputsCard);
    auto *actionsLayout = new QHBoxLayout(actionsRow);
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(8);
    auto *swapButton = new QPushButton(tr("⇅ Swap"), actionsRow);
    swapButton->setObjectName(QStringLiteral("SecondaryButton"));
    connect(swapButton, &QPushButton::clicked, this, [this] {
        const QString a = m_pathA->text();
        m_pathA->setText(m_pathB->text());
        m_pathB->setText(a);
    });
    m_compareButton = new QPushButton(tr("Compare"), actionsRow);
    m_compareButton->setObjectName(QStringLiteral("PrimaryButton"));
    connect(m_compareButton, &QPushButton::clicked, this, &ComparePage::onCompareClicked);
    auto *note = new QLabel(
        tr("compares min(a_slots, b_slots) and reports any slot-count mismatch — it is never "
           "silently hidden"),
        actionsRow);
    note->setObjectName(QStringLiteral("MutedNote"));
    note->setWordWrap(true);
    actionsLayout->addWidget(swapButton);
    actionsLayout->addWidget(m_compareButton);
    actionsLayout->addWidget(note, 1);
    inputsLayout->addWidget(actionsRow);
    layout->addWidget(inputsCard);

    // --- Summary strip (7 values from §11).
    auto *summaryCard = makeCard(QStringLiteral("GroupCard"), this);
    auto *summaryLayout = new QVBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(14, 13, 14, 13);
    summaryLayout->setSpacing(10);
    auto *summaryHeading = new QLabel(tr("SUMMARY"), summaryCard);
    summaryHeading->setObjectName(QStringLiteral("SectionHeading"));
    summaryLayout->addWidget(summaryHeading);
    auto *summaryGridHost = new QWidget(summaryCard);
    m_summaryGrid = buildSummaryStrip();
    summaryGridHost->setLayout(m_summaryGrid);
    summaryLayout->addWidget(summaryGridHost);
    layout->addWidget(summaryCard);

    // --- Delta table + changed slots.
    auto *bottomRow = new QWidget(this);
    auto *bottomLayout = new QHBoxLayout(bottomRow);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(12);

    auto *deltaCard = makeCard(QStringLiteral("GroupCard"), bottomRow);
    auto *deltaLayout = new QVBoxLayout(deltaCard);
    deltaLayout->setContentsMargins(14, 13, 14, 13);
    deltaLayout->setSpacing(10);
    auto *deltaHeading = new QLabel(tr("METRIC DELTAS (B − A)"), deltaCard);
    deltaHeading->setObjectName(QStringLiteral("SectionHeading"));
    deltaLayout->addWidget(deltaHeading);
    m_deltaTable = new QTableWidget(0, 4, deltaCard);
    m_deltaTable->setObjectName(QStringLiteral("CompareDeltaTable"));
    m_deltaTable->setHorizontalHeaderLabels({tr("metric"), tr("A"), tr("B"), tr("Δ")});
    m_deltaTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_deltaTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_deltaTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_deltaTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_deltaTable->verticalHeader()->setVisible(false);
    m_deltaTable->setEditTriggers(QTableWidget::NoEditTriggers);
    m_deltaTable->setSelectionBehavior(QTableWidget::SelectRows);
    deltaLayout->addWidget(m_deltaTable, 1);
    bottomLayout->addWidget(deltaCard, 1);

    auto *changedCard = makeCard(QStringLiteral("GroupCard"), bottomRow);
    auto *changedLayout = new QVBoxLayout(changedCard);
    changedLayout->setContentsMargins(14, 13, 14, 13);
    changedLayout->setSpacing(10);
    auto *changedHeadingRow = new QWidget(changedCard);
    auto *changedHeadingLayout = new QHBoxLayout(changedHeadingRow);
    changedHeadingLayout->setContentsMargins(0, 0, 0, 0);
    auto *changedHeading = new QLabel(tr("CHANGED SLOTS"), changedCard);
    changedHeading->setObjectName(QStringLiteral("SectionHeading"));
    m_changedCaption = new QLabel(changedHeadingRow);
    m_changedCaption->setObjectName(QStringLiteral("ColumnTag"));
    changedHeadingLayout->addWidget(changedHeading);
    changedHeadingLayout->addStretch(1);
    changedHeadingLayout->addWidget(m_changedCaption);
    changedLayout->addWidget(changedHeadingRow);
    m_changedTable = new QTableWidget(0, 2, changedCard);
    m_changedTable->setObjectName(QStringLiteral("CompareDeltaTable"));
    m_changedTable->setHorizontalHeaderLabels({tr("slot"), tr("hex A"), tr("hex B")});
    m_changedTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_changedTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_changedTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_changedTable->verticalHeader()->setVisible(false);
    m_changedTable->setEditTriggers(QTableWidget::NoEditTriggers);
    m_changedTable->setSelectionBehavior(QTableWidget::SelectRows);
    changedLayout->addWidget(m_changedTable, 1);
    bottomLayout->addWidget(changedCard, 1);

    layout->addWidget(bottomRow, 1);

    // --- Actions: copy / export the report.
    auto *actionsCard = makeCard(QStringLiteral("ResultBottomActions"), this);
    auto *actionsLayout2 = new QVBoxLayout(actionsCard);
    actionsLayout2->setContentsMargins(14, 13, 14, 13);
    actionsLayout2->setSpacing(8);
    auto *actionsRow2 = new QWidget(actionsCard);
    auto *actionsRowLayout = new QHBoxLayout(actionsRow2);
    actionsRowLayout->setContentsMargins(0, 0, 0, 0);
    actionsRowLayout->setSpacing(8);
    m_copyJsonButton = new QPushButton(tr("Copy as JSON"), actionsRow2);
    m_copyJsonButton->setObjectName(QStringLiteral("SecondaryButton"));
    connect(m_copyJsonButton, &QPushButton::clicked, this, [this] { copyResultJson(m_result); });
    m_exportJsonButton = new QPushButton(tr("Export report JSON"), actionsRow2);
    m_exportJsonButton->setObjectName(QStringLiteral("SecondaryButton"));
    connect(m_exportJsonButton, &QPushButton::clicked, this,
            [this] { exportResultJson(m_result); });
    auto *actionsNote = new QLabel(
        tr("report mirrors `crankl compare A B --json`; nothing is written to the archives"),
        actionsRow2);
    actionsNote->setObjectName(QStringLiteral("ActionsCaption"));
    actionsRowLayout->addWidget(m_copyJsonButton);
    actionsRowLayout->addWidget(m_exportJsonButton);
    actionsRowLayout->addWidget(actionsNote, 1);
    actionsLayout2->addWidget(actionsRow2);
    layout->addWidget(actionsCard);

    m_copyJsonButton->setEnabled(false);
    m_exportJsonButton->setEnabled(false);
}

QGridLayout *ComparePage::buildSummaryStrip() {
    static const char *const kKeys[] = {"slots compared",     "slots changed",   "hamming",
                                        "clifford resonance", "sheaf resonance", "Δ trit density",
                                        "Δ clifford energy"};
    auto *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);
    for (int i = 0; i < 7; ++i) {
        auto *cell = makeCard(QStringLiteral("CompareSummaryCell"));
        auto *cellLayout = new QVBoxLayout(cell);
        cellLayout->setContentsMargins(10, 8, 10, 8);
        cellLayout->setSpacing(3);
        auto *label = new QLabel(tr(kKeys[i]), cell);
        label->setObjectName(QStringLiteral("HealthCountLabel"));
        cellLayout->addWidget(label);
        cellLayout->addWidget(makeSummaryValue(QStringLiteral("—"), cell));
        grid->addWidget(cell, i / 4, i % 4);
    }
    return grid;
}

void ComparePage::onCompareClicked() {
    if (!pathsReady()) {
        QMessageBox::warning(this, tr("Compare"), tr("Choose two .crank archives to compare."));
        return;
    }
    setBusy(true);
    Q_EMIT compareRequested(m_pathA->text(), m_pathB->text());
}

bool ComparePage::pathsReady() const {
    const QString a = m_pathA->text();
    const QString b = m_pathB->text();
    return !a.isEmpty() && !b.isEmpty();
}

void ComparePage::setArchiveA(const QString &path) {
    m_pathA->setText(path);
}

void ComparePage::setBusy(bool busy) {
    m_compareButton->setEnabled(!busy);
    m_compareButton->setText(busy ? tr("Comparing…") : tr("Compare"));
}

void ComparePage::clear() {
    m_pathA->clear();
    m_pathB->clear();
    setBusy(false);
    m_result = CompareResult{};
    updateSummary(m_result);
    m_deltaTable->setRowCount(0);
    m_changedTable->setRowCount(0);
    m_changedCaption->clear();
    m_copyJsonButton->setEnabled(false);
    m_exportJsonButton->setEnabled(false);
}

void ComparePage::showResult(const CompareResult &result) {
    m_result = result;
    setBusy(false);

    if (!result.ok) {
        QMessageBox::warning(this, tr("Compare failed"), result.errorMessage);
        updateSummary(CompareResult{});
        m_deltaTable->setRowCount(0);
        m_changedTable->setRowCount(0);
        m_changedCaption->clear();
        m_copyJsonButton->setEnabled(false);
        m_exportJsonButton->setEnabled(false);
        return;
    }

    updateSummary(result);
    populateDeltaTable(result);
    populateChangedList(result);
    m_copyJsonButton->setEnabled(true);
    m_exportJsonButton->setEnabled(true);
}

void ComparePage::updateSummary(const CompareResult &result) {
    const double deltaTritDensity =
        result.ok ? result.metricsB.tritDensity - result.metricsA.tritDensity : 0.0;
    const double deltaEnergy =
        result.ok ? result.metricsB.cliffordEnergy - result.metricsA.cliffordEnergy : 0.0;
    const QStringList values = {
        result.ok ? QString::number(result.slotsCompared) : QStringLiteral("—"),
        result.ok ? QStringLiteral("%1 / %2").arg(result.slotsChanged).arg(result.slotsCompared)
                  : QStringLiteral("—"),
        result.ok ? QString::number(result.hamming, 'f', 4) : QStringLiteral("—"),
        result.ok ? QString::number(result.cliffordResonance, 'f', 4) : QStringLiteral("—"),
        result.ok ? QString::number(result.sheafResonance, 'f', 4) : QStringLiteral("—"),
        result.ok ? deltaText(deltaTritDensity) : QStringLiteral("—"),
        result.ok ? deltaText(deltaEnergy) : QStringLiteral("—"),
    };

    int row = 0;
    for (int i = 0; i < m_summaryGrid->count(); ++i) {
        QLayoutItem *item = m_summaryGrid->itemAt(i);
        if (QWidget *cell = item->widget()) {
            QLayout *cellLayout = cell->layout();
            if (cellLayout && cellLayout->count() == 2) {
                auto *valueLabel = qobject_cast<QLabel *>(cellLayout->itemAt(1)->widget());
                if (valueLabel) {
                    valueLabel->setText(values[row]);
                    setStyleProperty(valueLabel, "dim", !result.ok);
                }
            }
            ++row;
        }
    }
}

void ComparePage::populateDeltaTable(const CompareResult &result) {
    const ArchiveMetrics &a = result.metricsA;
    const ArchiveMetrics &b = result.metricsB;
    struct RowSpec {
        const char *name;
        QString aText;
        QString bText;
        QString deltaText;
    };
    const auto fmt = [](double v) { return QString::number(v, 'f', 6); };
    const RowSpec rows[] = {
        {"n_slots", QString::number(a.nSlots), QString::number(b.nSlots),
         deltaText(static_cast<double>(b.nSlots) - static_cast<double>(a.nSlots))},
        {"depth_min", QString::number(a.depthMin), QString::number(b.depthMin),
         deltaText(static_cast<double>(b.depthMin) - static_cast<double>(a.depthMin))},
        {"depth_max", QString::number(a.depthMax), QString::number(b.depthMax),
         deltaText(static_cast<double>(b.depthMax) - static_cast<double>(a.depthMax))},
        {"scalar_mean", fmt(a.scalarMean), fmt(b.scalarMean),
         deltaText(b.scalarMean - a.scalarMean)},
        {"scalar_abs_mean", fmt(a.scalarAbsMean), fmt(b.scalarAbsMean),
         deltaText(b.scalarAbsMean - a.scalarAbsMean)},
        {"trit_density", fmt(a.tritDensity), fmt(b.tritDensity),
         deltaText(b.tritDensity - a.tritDensity)},
        {"trit_entropy", fmt(a.tritEntropy), fmt(b.tritEntropy),
         deltaText(b.tritEntropy - a.tritEntropy)},
        {"clifford_energy", fmt(a.cliffordEnergy), fmt(b.cliffordEnergy),
         deltaText(b.cliffordEnergy - a.cliffordEnergy)},
        {"beta1_proxy", fmt(a.beta1Proxy), fmt(b.beta1Proxy),
         deltaText(b.beta1Proxy - a.beta1Proxy)},
    };
    m_deltaTable->setRowCount(static_cast<int>(std::size(rows)));
    for (int i = 0; i < static_cast<int>(std::size(rows)); ++i) {
        const RowSpec &spec = rows[i];
        m_deltaTable->setItem(i, 0, new QTableWidgetItem(QLatin1String(spec.name)));
        m_deltaTable->setItem(i, 1, new QTableWidgetItem(spec.aText));
        m_deltaTable->setItem(i, 2, new QTableWidgetItem(spec.bText));
        m_deltaTable->setItem(i, 3, new QTableWidgetItem(spec.deltaText));
    }
}

void ComparePage::populateChangedList(const CompareResult &result) {
    m_changedTable->setRowCount(static_cast<int>(result.changedSlots.size()));
    for (int i = 0; i < static_cast<int>(result.changedSlots.size()); ++i) {
        const CompareResult::ChangedSlot &slot = result.changedSlots[static_cast<size_t>(i)];
        const auto hex = [](uint64_t word) {
            return QStringLiteral("0x") +
                   QString::number(word, 16).toUpper().rightJustified(16, QLatin1Char('0'));
        };
        m_changedTable->setItem(i, 0, new QTableWidgetItem(QString::number(slot.index)));
        m_changedTable->setItem(i, 1, new QTableWidgetItem(hex(slot.wordA)));
        m_changedTable->setItem(i, 2, new QTableWidgetItem(hex(slot.wordB)));
    }
    const size_t total = result.slotsChanged;
    const size_t shown = result.changedSlots.size();
    m_changedCaption->setText(total == 0      ? tr("no slots differ")
                              : shown < total ? tr("showing first %1 of %2").arg(shown).arg(total)
                                              : tr("%1 slots").arg(total));
}

void ComparePage::copyResultJson(const CompareResult &result) {
    if (!result.ok)
        return;
    QJsonObject obj;
    obj.insert(QStringLiteral("a"), result.pathA);
    obj.insert(QStringLiteral("b"), result.pathB);
    obj.insert(QStringLiteral("slots_changed"), static_cast<qint64>(result.slotsChanged));
    obj.insert(QStringLiteral("slots_compared"), static_cast<qint64>(result.slotsCompared));
    obj.insert(QStringLiteral("slots_a"), static_cast<qint64>(result.slotsA));
    obj.insert(QStringLiteral("slots_b"), static_cast<qint64>(result.slotsB));
    obj.insert(QStringLiteral("hamming"), result.hamming);
    obj.insert(QStringLiteral("clifford_resonance"), result.cliffordResonance);
    obj.insert(QStringLiteral("sheaf_resonance"), result.sheafResonance);
    obj.insert(QStringLiteral("delta_trit_density"),
               result.metricsB.tritDensity - result.metricsA.tritDensity);
    obj.insert(QStringLiteral("delta_energy"),
               result.metricsB.cliffordEnergy - result.metricsA.cliffordEnergy);
    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    QApplication::clipboard()->setText(QString::fromUtf8(payload));
}

void ComparePage::exportResultJson(const CompareResult &result) {
    if (!result.ok)
        return;
    const QString suggested =
        QFileInfo(result.pathA).completeBaseName() + QStringLiteral("__vs__") +
        QFileInfo(result.pathB).completeBaseName() + QStringLiteral(".compare.json");
    const QString path = QFileDialog::getSaveFileName(this, tr("Export comparison report"),
                                                      suggested, tr("JSON (*.json)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, tr("Export failed"),
                              tr("Could not open %1 for writing:\n%2")
                                  .arg(QDir::toNativeSeparators(path), file.errorString()));
        return;
    }
    QJsonObject obj;
    obj.insert(QStringLiteral("a"), result.pathA);
    obj.insert(QStringLiteral("b"), result.pathB);
    obj.insert(QStringLiteral("slots_changed"), static_cast<qint64>(result.slotsChanged));
    obj.insert(QStringLiteral("slots_compared"), static_cast<qint64>(result.slotsCompared));
    obj.insert(QStringLiteral("hamming"), result.hamming);
    obj.insert(QStringLiteral("clifford_resonance"), result.cliffordResonance);
    obj.insert(QStringLiteral("sheaf_resonance"), result.sheafResonance);
    obj.insert(QStringLiteral("delta_trit_density"),
               result.metricsB.tritDensity - result.metricsA.tritDensity);
    obj.insert(QStringLiteral("delta_energy"),
               result.metricsB.cliffordEnergy - result.metricsA.cliffordEnergy);
    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size() || !file.flush()) {
        QMessageBox::critical(
            this, tr("Export failed"),
            tr("Could not write %1:\n%2").arg(QDir::toNativeSeparators(path), file.errorString()));
        file.close();
        return;
    }
    file.close();
}

void ComparePage::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void ComparePage::dropEvent(QDropEvent *event) {
    const auto urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;
    const QString path = urls.first().toLocalFile();
    if (path.isEmpty())
        return;
    event->acceptProposedAction();
    setArchiveA(path);
}

} // namespace crankl_gui