#ifndef CRANKL_GUI_WIDGETS_SLOT_BROWSER_H
#define CRANKL_GUI_WIDGETS_SLOT_BROWSER_H

#include "core/ArchiveSnapshot.h"

#include <QFrame>
#include <QVector>

class QLabel;
class QStackedWidget;
class QTableWidget;
class QGridLayout;

namespace crankl_gui {

class HeatmapWidget;

// Center-column preview card. Two top-level states:
//
// SlotBrowser never touches the C API's I/O surface -- it only decodes
// (crankl_crank_to_multivector / crankl_decrank_matrix) from crank words
// already copied into the ArchiveSnapshot it was given.
class SlotBrowser : public QFrame {
    Q_OBJECT
public:
    explicit SlotBrowser(QWidget *parent = nullptr);

    void setSnapshot(const ArchiveSnapshot &snapshot); // switches to loaded state
    void setSlotIndex(int index);
    void setNumericMode(bool numeric); // Heatmap/Numeric toggle (design 5d)
    void setCondensed(bool condensed); // narrow pages: coefficients 2-up, tags hidden
    void clear();                       // switches to the 4a empty state

Q_SIGNALS:
    void openArchiveRequested();
    void recentRequested();

private:
    QWidget *buildEmptyPage();
    QWidget *buildLoadedPage();
    void refreshSlot();
    void refreshDepthHistogram();
    void rebuildBitGrid(uint64_t word);

    ArchiveSnapshot m_snapshot;
    int m_slotIndex = 0;

    QLabel *m_title = nullptr;
    QLabel *m_slotTag = nullptr;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_emptyPage = nullptr;
    QWidget *m_loadedPage = nullptr;

    QStackedWidget *m_viewStack = nullptr; // heatmap view | numeric view
    HeatmapWidget *m_heatmap = nullptr;
    QLabel *m_legendMin = nullptr;
    QLabel *m_legendMax = nullptr;
    QTableWidget *m_numericTable = nullptr;
    QLabel *m_wordHex = nullptr;
    QWidget *m_bitGrid = nullptr;
    QVector<QLabel *> m_coefficientValues; // e0..e0123 in order
    QVector<QWidget *> m_coeffCells;       // the bordered cells, for re-flow
    QGridLayout *m_coeffGridLayout = nullptr;
    QLabel *m_colTag = nullptr;
    bool m_condensed = false;
    QWidget *m_depthHistogram = nullptr;
    QLabel *m_depthCaption = nullptr;
};

} // namespace crankl_gui

#endif // CRANKL_GUI_WIDGETS_SLOT_BROWSER_H
