#ifndef CRANKL_GUI_WIDGETS_METRICS_PANEL_H
#define CRANKL_GUI_WIDGETS_METRICS_PANEL_H

#include "core/ArchiveSnapshot.h"

#include <QFrame>
#include <QVector>

class QLabel;

namespace crankl_gui {

// §5.2 / design 5a: the nine archive-metric rows in fixed order (matching
// the C struct field order, so the panel is scannable against
// `crankl inspect --json` line for line), with a tooltip per row (5b texts)
// and the beta1_proxy callout.
class MetricsPanel : public QFrame {
    Q_OBJECT
  public:
    explicit MetricsPanel(QWidget *parent = nullptr);

    void setMetrics(const ArchiveMetrics &metrics);
    void setPending();
    void clear();

  private:
    enum class State { Loaded, Empty, Pending };
    void applyState(State state);

    struct Row {
        QLabel *key = nullptr;
        QLabel *value = nullptr;
    };
    QVector<Row> m_rows; // fixed order: n_slots .. beta1_proxy
    QLabel *m_proxyCallout = nullptr;
    QLabel *m_emptyNote = nullptr;
};

} // namespace crankl_gui

#endif // CRANKL_GUI_WIDGETS_METRICS_PANEL_H
