#ifndef CRANKL_GUI_WIDGETS_RESULT_SUMMARY_PANEL_H
#define CRANKL_GUI_WIDGETS_RESULT_SUMMARY_PANEL_H

#include "core/ArchiveSnapshot.h"

#include <QFrame>

class QLabel;
class QPushButton;

namespace crankl_gui {

class MetricsPanel;

// Right-column content for InspectPage's frame (3a loaded / 4a empty): the
// RESULT SUMMARY heading, the nine metric rows, the rollback-unavailable
// note (loaded only), and a bottom action slot holding two read-only
// exports (Copy as JSON, Export stats report). Neither export writes
// through any crankl_cran_write* path -- InspectPage serializes the
// already-fetched snapshot in response to these signals. When no archive is
// open, both actions are disabled with the caption "available once an
// archive is open".
//
// The bottom action slot is handed to ThreeColumnPage via
// setBottomActions(): a future write-capable phase reusing this frame swaps
// in a Run button there without moving anything else.
class ResultSummaryPanel : public QFrame {
    Q_OBJECT
  public:
    explicit ResultSummaryPanel(QWidget *parent = nullptr);
    ~ResultSummaryPanel() override;

    void setSnapshot(const ArchiveSnapshot &snapshot);
    void setPending();
    void clear();

    QWidget *bottomActionsWidget() const {
        return m_bottomActions;
    }

  Q_SIGNALS:
    void copyAsJsonRequested();
    void exportStatsReportRequested();

  private:
    QWidget *buildBottomActions();

    MetricsPanel *m_metrics = nullptr;
    QLabel *m_rollbackNote = nullptr;
    QWidget *m_bottomActions = nullptr;
    QPushButton *m_copyJsonButton = nullptr;
    QPushButton *m_exportButton = nullptr;
    QLabel *m_actionsCaption = nullptr;
};

} // namespace crankl_gui

#endif // CRANKL_GUI_WIDGETS_RESULT_SUMMARY_PANEL_H
