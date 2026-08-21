#ifndef CRANKL_GUI_WIDGETS_RESULT_SUMMARY_PANEL_H
#define CRANKL_GUI_WIDGETS_RESULT_SUMMARY_PANEL_H

#include "core/ArchiveSnapshot.h"

#include <QFrame>
#include <QJsonObject>

class QLabel;
class QPushButton;
class QPlainTextEdit;

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

    // Serializes the loaded snapshot as `crankl inspect --json` would, for
    // the preview pane and for copy/export.
    static QJsonObject snapshotToJson(const ArchiveSnapshot &snapshot);

    QWidget *bottomActionsWidget() const {
        return m_bottomActions;
    }

  Q_SIGNALS:
    void copyAsJsonRequested();
    void exportStatsReportRequested();

  private:
    QWidget *buildBottomActions();
    void updateJsonPreview();

    MetricsPanel *m_metrics = nullptr;
    QLabel *m_rollbackNote = nullptr;
    QPlainTextEdit *m_jsonPreview = nullptr;
    QWidget *m_bottomActions = nullptr;
    QPushButton *m_copyJsonButton = nullptr;
    QPushButton *m_exportButton = nullptr;
    QLabel *m_actionsCaption = nullptr;
    ArchiveSnapshot m_snapshot;
};

} // namespace crankl_gui

#endif // CRANKL_GUI_WIDGETS_RESULT_SUMMARY_PANEL_H
