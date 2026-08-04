#include "widgets/SlotBrowser.h"

#include "widgets/HeatmapWidget.h"
#include "widgets/StyleUtil.h"

#include "crankl/crank.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <utility>

namespace crankl_gui {

namespace {
// Numeric-view sign colors (design 5d): cyan positive, orange negative.
const QColor kPositiveColor(0x4f, 0xc3, 0xda);
const QColor kNegativeColor(0xdd, 0x8a, 0x5e);
const QColor kZeroColor(0x8d, 0x95, 0x9b);
} // namespace

SlotBrowser::SlotBrowser(QWidget *parent) : QFrame(parent) {
    setObjectName(QStringLiteral("SlotBrowser"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(13);

    auto *headerRow = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(11);
    m_title = new QLabel(tr("Slot browser"), headerRow);
    m_title->setObjectName(QStringLiteral("CardTitle"));
    m_slotTag = new QLabel(headerRow);
    m_slotTag->setObjectName(QStringLiteral("SlotTag"));
    m_colTag = new QLabel(tr("column 2 · preview"), headerRow);
    m_colTag->setObjectName(QStringLiteral("ColumnTag"));
    headerLayout->addWidget(m_title);
    headerLayout->addWidget(m_slotTag);
    headerLayout->addStretch(1);
    headerLayout->addWidget(m_colTag);
    layout->addWidget(headerRow);

    m_stack = new QStackedWidget(this);
    m_emptyPage = buildEmptyPage();
    m_loadedPage = buildLoadedPage();
    m_stack->addWidget(m_emptyPage);
    m_stack->addWidget(m_loadedPage);
    layout->addWidget(m_stack, 1);

    clear();
}

QWidget *SlotBrowser::buildEmptyPage() {
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto *dropFrame = makeCard(QStringLiteral("InspectDropFrame"), page);
    auto *dropLayout = new QVBoxLayout(dropFrame);
    dropLayout->setAlignment(Qt::AlignCenter);
    dropLayout->setSpacing(14);

    // Decorative bit grid: deterministic scatter of accent cells at 35%
    // opacity (4a). No randomness at runtime -- pattern is a fixed hash.
    auto *decoration = new QWidget(dropFrame);
    auto *decoLayout = new QGridLayout(decoration);
    decoLayout->setSpacing(3);
    for (int i = 0; i < 64; ++i) {
        auto *cell = makeCard(QStringLiteral("DecoBitCell"), decoration);
        cell->setFixedSize(10, 10);
        const bool lit = ((i * 2654435761u) >> 7) % 5 < 2;
        setStyleProperty(cell, "lit", lit);
        decoLayout->addWidget(cell, i / 8, i % 8);
    }
    dropLayout->addWidget(decoration, 0, Qt::AlignHCenter);

    auto *title = new QLabel(tr("Drop a .crank archive to inspect it"), dropFrame);
    title->setObjectName(QStringLiteral("DropTitle"));
    title->setAlignment(Qt::AlignCenter);
    dropLayout->addWidget(title);

    auto *explainer = new QLabel(
        tr("Inspect reads the archive, copies out metrics and slot words, then closes the "
           "mapping. The file itself is never modified."),
        dropFrame);
    explainer->setObjectName(QStringLiteral("DropExplainer"));
    explainer->setWordWrap(true);
    explainer->setAlignment(Qt::AlignCenter);
    explainer->setMaximumWidth(400);
    explainer->setMinimumWidth(240);
    dropLayout->addWidget(explainer, 0, Qt::AlignHCenter);

    auto *buttonsRow = new QWidget(dropFrame);
    auto *buttonsLayout = new QHBoxLayout(buttonsRow);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->setSpacing(9);
    auto *openButton = new QPushButton(tr("Open archive…"), buttonsRow);
    openButton->setObjectName(QStringLiteral("PrimaryButton"));
    connect(openButton, &QPushButton::clicked, this, &SlotBrowser::openArchiveRequested);
    auto *recentButton = new QPushButton(tr("Recent…"), buttonsRow);
    recentButton->setObjectName(QStringLiteral("SecondaryButton"));
    connect(recentButton, &QPushButton::clicked, this, &SlotBrowser::recentRequested);
    buttonsLayout->addWidget(openButton);
    buttonsLayout->addWidget(recentButton);
    dropLayout->addWidget(buttonsRow, 0, Qt::AlignHCenter);

    auto *caption = new QLabel(tr("accepts .crank only · one archive open at a time"), dropFrame);
    caption->setObjectName(QStringLiteral("DropCaption"));
    caption->setAlignment(Qt::AlignCenter);
    caption->setWordWrap(true);
    dropLayout->addWidget(caption);

    layout->addWidget(dropFrame, 1);

    auto *noteBar = makeCard(QStringLiteral("WeightFileNote"), page);
    auto *noteLayout = new QHBoxLayout(noteBar);
    noteLayout->setContentsMargins(12, 10, 12, 10);
    noteLayout->setSpacing(9);
    auto *noteRail = makeCard(QStringLiteral("NoteRail"), noteBar);
    noteRail->setFixedWidth(3);
    auto *noteText = new QLabel(
        tr("Weight files can be dropped here, but this build cannot read them — you'll get an "
           "explanation, not an import."),
        noteBar);
    noteText->setObjectName(QStringLiteral("NoteText"));
    noteText->setWordWrap(true);
    noteLayout->addWidget(noteRail);
    noteLayout->addWidget(noteText, 1);
    layout->addWidget(noteBar);

    return page;
}

QWidget *SlotBrowser::buildLoadedPage() {
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    m_viewStack = new QStackedWidget(page);

    // --- Heatmap view (3a): 212px heat column + word/coefficients column.
    auto *heatView = new QWidget(m_viewStack);
    auto *heatViewLayout = new QHBoxLayout(heatView);
    heatViewLayout->setContentsMargins(0, 0, 0, 0);
    heatViewLayout->setSpacing(16);

    auto *heatColumn = new QWidget(heatView);
    heatColumn->setFixedWidth(212);
    auto *heatColumnLayout = new QVBoxLayout(heatColumn);
    heatColumnLayout->setContentsMargins(0, 0, 0, 0);
    heatColumnLayout->setSpacing(9);
    auto *heatHeading = new QLabel(tr("DECODED 8×8 BLOCK"), heatColumn);
    heatHeading->setObjectName(QStringLiteral("SectionHeading"));
    m_heatmap = new HeatmapWidget(heatColumn);
    m_heatmap->setFixedSize(212, 212);
    auto *legendRow = new QWidget(heatColumn);
    auto *legendLayout = new QHBoxLayout(legendRow);
    legendLayout->setContentsMargins(0, 0, 0, 0);
    legendLayout->setSpacing(8);
    m_legendMin = new QLabel(legendRow);
    m_legendMin->setObjectName(QStringLiteral("LegendLabel"));
    auto *legendBar = makeCard(QStringLiteral("HeatmapLegendBar"), legendRow);
    legendBar->setFixedHeight(7);
    m_legendMax = new QLabel(legendRow);
    m_legendMax->setObjectName(QStringLiteral("LegendLabel"));
    legendLayout->addWidget(m_legendMin);
    legendLayout->addWidget(legendBar, 1);
    legendLayout->addWidget(m_legendMax);
    heatColumnLayout->addWidget(heatHeading);
    heatColumnLayout->addWidget(m_heatmap);
    heatColumnLayout->addWidget(legendRow);
    heatColumnLayout->addStretch(1);
    heatViewLayout->addWidget(heatColumn);

    auto *detailColumn = new QWidget(heatView);
    auto *detailLayout = new QVBoxLayout(detailColumn);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(9);
    auto *wordHeading = new QLabel(tr("CRANK WORD"), detailColumn);
    wordHeading->setObjectName(QStringLiteral("SectionHeading"));
    detailLayout->addWidget(wordHeading);

    // Bit grid with the hex word stacked below it (the 3b arrangement).
    // Keeping them vertical rather than side-by-side also keeps this
    // column's minimum width small enough that the page can actually shrink
    // to the narrow-reflow threshold.
    auto *wordGroup = new QWidget(detailColumn);
    auto *wordGroupLayout = new QVBoxLayout(wordGroup);
    wordGroupLayout->setContentsMargins(0, 0, 0, 0);
    wordGroupLayout->setSpacing(8);
    m_bitGrid = new QWidget(wordGroup);
    m_wordHex = new QLabel(wordGroup);
    m_wordHex->setObjectName(QStringLiteral("WordHex"));
    wordGroupLayout->addWidget(m_bitGrid, 0, Qt::AlignLeft);
    wordGroupLayout->addWidget(m_wordHex);
    detailLayout->addWidget(wordGroup);

    detailLayout->addSpacing(5);
    auto *coeffHeading = new QLabel(tr("MULTIVECTOR COEFFICIENTS"), detailColumn);
    coeffHeading->setObjectName(QStringLiteral("SectionHeading"));
    detailLayout->addWidget(coeffHeading);

    auto *coeffGrid = new QWidget(detailColumn);
    m_coeffGridLayout = new QGridLayout(coeffGrid);
    auto *coeffGridLayout = m_coeffGridLayout;
    coeffGridLayout->setContentsMargins(0, 0, 0, 0);
    coeffGridLayout->setSpacing(6);
    static const QStringList kCoeffNames = {QStringLiteral("e0"),  QStringLiteral("e1"),
                                            QStringLiteral("e2"),  QStringLiteral("e3"),
                                            QStringLiteral("e01"), QStringLiteral("e02"),
                                            QStringLiteral("e03"), QStringLiteral("e0123")};
    for (int i = 0; i < kCoeffNames.size(); ++i) {
        auto *cell = makeCard(QStringLiteral("CoeffCell"), coeffGrid);
        auto *cellLayout = new QVBoxLayout(cell);
        cellLayout->setContentsMargins(8, 6, 8, 6);
        cellLayout->setSpacing(3);
        auto *nameLabel = new QLabel(kCoeffNames[i], cell);
        nameLabel->setObjectName(QStringLiteral("CoeffName"));
        auto *valueLabel = new QLabel(cell);
        valueLabel->setObjectName(QStringLiteral("CoeffValue"));
        cellLayout->addWidget(nameLabel);
        cellLayout->addWidget(valueLabel);
        coeffGridLayout->addWidget(cell, i / 4, i % 4);
        m_coefficientValues.push_back(valueLabel);
        m_coeffCells.push_back(cell);
    }
    detailLayout->addWidget(coeffGrid);
    detailLayout->addStretch(1);
    heatViewLayout->addWidget(detailColumn, 1);
    m_viewStack->addWidget(heatView);

    // --- Numeric view (5d): full-width 8x8 table, 4 dp, row-major,
    // selectable and copyable, sign-colored.
    auto *numericView = new QWidget(m_viewStack);
    auto *numericLayout = new QVBoxLayout(numericView);
    numericLayout->setContentsMargins(0, 0, 0, 0);
    numericLayout->setSpacing(9);
    auto *numericHeading = new QLabel(tr("DECODED 8×8 BLOCK"), numericView);
    numericHeading->setObjectName(QStringLiteral("SectionHeading"));
    numericLayout->addWidget(numericHeading);

    m_numericTable = new QTableWidget(8, 8, numericView);
    m_numericTable->setObjectName(QStringLiteral("NumericTable"));
    m_numericTable->horizontalHeader()->setVisible(false);
    m_numericTable->verticalHeader()->setVisible(false);
    m_numericTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_numericTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_numericTable->verticalHeader()->setDefaultSectionSize(26);
    m_numericTable->setEditTriggers(QTableWidget::NoEditTriggers);
    m_numericTable->setSelectionMode(QTableWidget::ContiguousSelection);
    m_numericTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_numericTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_numericTable->setFixedHeight(8 * 26 + 2);
    numericLayout->addWidget(m_numericTable);

    auto *numericLegend = new QWidget(numericView);
    auto *numericLegendLayout = new QHBoxLayout(numericLegend);
    numericLegendLayout->setContentsMargins(0, 0, 0, 0);
    numericLegendLayout->setSpacing(14);
    auto addChip = [&](const QString &objectName, const QString &text) {
        auto *chipRow = new QWidget(numericLegend);
        auto *chipLayout = new QHBoxLayout(chipRow);
        chipLayout->setContentsMargins(0, 0, 0, 0);
        chipLayout->setSpacing(6);
        auto *chip = makeCard(objectName, chipRow);
        chip->setFixedSize(9, 9);
        auto *label = new QLabel(text, chipRow);
        label->setObjectName(QStringLiteral("LegendLabel"));
        chipLayout->addWidget(chip);
        chipLayout->addWidget(label);
        numericLegendLayout->addWidget(chipRow);
    };
    addChip(QStringLiteral("PositiveChip"), tr("positive"));
    addChip(QStringLiteral("NegativeChip"), tr("negative"));
    numericLegendLayout->addStretch(1);
    auto *numericCaption =
        new QLabel(tr("4 dp · row-major · QTableView, selectable and copyable"), numericLegend);
    numericCaption->setObjectName(QStringLiteral("LegendLabel"));
    numericCaption->setWordWrap(true);
    numericLegendLayout->addWidget(numericCaption);
    numericLayout->addWidget(numericLegend);
    numericLayout->addStretch(1);
    m_viewStack->addWidget(numericView);

    layout->addWidget(m_viewStack);

    // --- Depth distribution (3a), below whichever view is active.
    auto *depthHeadingRow = new QWidget(page);
    auto *depthHeadingLayout = new QHBoxLayout(depthHeadingRow);
    depthHeadingLayout->setContentsMargins(0, 0, 0, 0);
    depthHeadingLayout->setSpacing(11);
    auto *depthHeading = new QLabel(tr("DEPTH DISTRIBUTION"), depthHeadingRow);
    depthHeading->setObjectName(QStringLiteral("SectionHeading"));
    m_depthCaption = new QLabel(depthHeadingRow);
    m_depthCaption->setObjectName(QStringLiteral("ColumnTag"));
    depthHeadingLayout->addWidget(depthHeading);
    depthHeadingLayout->addWidget(m_depthCaption);
    depthHeadingLayout->addStretch(1);
    layout->addWidget(depthHeadingRow);

    m_depthHistogram = new QWidget(page);
    m_depthHistogram->setFixedHeight(70);
    layout->addWidget(m_depthHistogram);
    layout->addStretch(1);

    auto *footnote = new QLabel(
        tr("The block is decoded from the single word above — a reconstruction, not the "
           "original floats. Slot words were copied out before the mapping was released."),
        page);
    footnote->setObjectName(QStringLiteral("NoteText"));
    footnote->setWordWrap(true);
    layout->addWidget(footnote);

    return page;
}

void SlotBrowser::setSnapshot(const ArchiveSnapshot &snapshot) {
    m_snapshot = snapshot;
    m_slotIndex = 0;
    setStyleProperty(m_title, "empty", false);
    m_stack->setCurrentWidget(m_loadedPage);
    refreshDepthHistogram();
    refreshSlot();
}

void SlotBrowser::setSlotIndex(int index) {
    m_slotIndex = index;
    refreshSlot();
}

void SlotBrowser::setNumericMode(bool numeric) {
    m_viewStack->setCurrentIndex(numeric ? 1 : 0);
}

void SlotBrowser::setCondensed(bool condensed) {
    if (m_condensed == condensed)
        return;
    m_condensed = condensed;
    // Re-flow the coefficient cells 4-up <-> 2-up. This is what keeps the
    // center column's minimum width below the narrow-page floor -- a
    // 4-across grid alone would force horizontal clipping at 1024px.
    const int columns = condensed ? 2 : 4;
    for (int i = 0; i < m_coeffCells.size(); ++i) {
        m_coeffGridLayout->removeWidget(m_coeffCells[i]);
        m_coeffGridLayout->addWidget(m_coeffCells[i], i / columns, i % columns);
    }
    m_colTag->setVisible(!condensed);
}

void SlotBrowser::clear() {
    m_snapshot = ArchiveSnapshot{};
    m_slotIndex = 0;
    m_slotTag->clear();
    setStyleProperty(m_title, "empty", true);
    m_stack->setCurrentWidget(m_emptyPage);
}

void SlotBrowser::refreshSlot() {
    if (m_snapshot.crankWords.empty() || m_slotIndex < 0 ||
        static_cast<size_t>(m_slotIndex) >= m_snapshot.crankWords.size()) {
        return;
    }
    const uint64_t word = m_snapshot.crankWords[static_cast<size_t>(m_slotIndex)];
    m_slotTag->setText(tr("slot %1").arg(m_slotIndex));

    std::array<double, 64> block{};
    crankl_decrank_matrix(word, block.data());
    m_heatmap->setValues(block);
    const double maxAbs = m_heatmap->maxAbsValue();
    m_legendMin->setText(QStringLiteral("−%1").arg(maxAbs, 0, 'f', 2));
    m_legendMax->setText(QStringLiteral("+%1").arg(maxAbs, 0, 'f', 2));

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const double value = block[static_cast<size_t>(row * 8 + col)];
            auto *item = new QTableWidgetItem(QString::number(value, 'f', 4));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            item->setForeground(value > 0   ? kPositiveColor
                                : value < 0 ? kNegativeColor
                                            : kZeroColor);
            m_numericTable->setItem(row, col, item);
        }
    }

    m_wordHex->setText(QStringLiteral("0x") +
                       QString::number(word, 16).toUpper().rightJustified(16, QLatin1Char('0')));
    rebuildBitGrid(word);

    crankl_multivector_t mv{};
    uint8_t depth = 0;
    crankl_crank_to_multivector(word, &mv, &depth);
    const std::array<double, 8> coeffs = {mv.s,    mv.v[0], mv.v[1], mv.v[2],
                                          mv.b[0], mv.b[1], mv.b[2], mv.p};
    for (int i = 0; i < static_cast<int>(coeffs.size()) && i < m_coefficientValues.size(); ++i) {
        const double v = coeffs[static_cast<size_t>(i)];
        m_coefficientValues[i]->setText(QStringLiteral("%1%2").arg(
            v >= 0 ? QStringLiteral("+") : QString(), QString::number(v, 'f', 4)));
    }
}

void SlotBrowser::rebuildBitGrid(uint64_t word) {
    QLayout *existing = m_bitGrid->layout();
    if (!existing) {
        auto *grid = new QGridLayout(m_bitGrid);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(2);
        existing = grid;
        for (int bit = 0; bit < 64; ++bit) {
            auto *cell = makeCard(QStringLiteral("BitCell"), m_bitGrid);
            cell->setFixedSize(7, 7);
            grid->addWidget(cell, bit / 16, bit % 16);
        }
    }
    auto *grid = static_cast<QGridLayout *>(existing);
    for (int bit = 0; bit < 64; ++bit) {
        QWidget *cell = grid->itemAtPosition(bit / 16, bit % 16)->widget();
        setStyleProperty(cell, "bitSet", ((word >> bit) & 1ULL) != 0);
    }
}

void SlotBrowser::refreshDepthHistogram() {
    QLayout *existing = m_depthHistogram->layout();
    if (!existing) {
        auto *rowLayout = new QHBoxLayout(m_depthHistogram);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(3);
        existing = rowLayout;
    } else {
        QLayoutItem *item;
        while ((item = existing->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
    }
    auto *layout = static_cast<QHBoxLayout *>(existing);

    if (m_snapshot.crankWords.empty()) {
        m_depthCaption->clear();
        return;
    }

    QMap<uint8_t, int> counts;
    for (uint64_t word : m_snapshot.crankWords) {
        crankl_multivector_t mv{};
        uint8_t depth = 0;
        crankl_crank_to_multivector(word, &mv, &depth);
        counts[depth] += 1;
    }

    int maxCount = 0;
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
        maxCount = std::max(maxCount, it.value());

    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        auto *bar = makeCard(QStringLiteral("DepthBar"), m_depthHistogram);
        const int heightPx = maxCount > 0 ? std::max(6, it.value() * 70 / maxCount) : 6;
        bar->setFixedSize(22, heightPx);
        bar->setToolTip(tr("depth %1 · %2 slots").arg(it.key()).arg(it.value()));
        layout->addWidget(bar, 0, Qt::AlignBottom);
    }
    layout->addStretch(1);

    m_depthCaption->setText(tr("all %1 slots").arg(m_snapshot.crankWords.size()));
}

} // namespace crankl_gui
