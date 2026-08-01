#ifndef CRANKL_GUI_WIDGETS_HEATMAP_WIDGET_H
#define CRANKL_GUI_WIDGETS_HEATMAP_WIDGET_H

#include <QWidget>

#include <array>

class QPaintEvent;

namespace crankl_gui {

// Generic reusable 8x8 colored-cell grid.
//
// Values are mapped to a diverging orange/cyan scale around zero,
// autoscaled to the current block's largest magnitude -- matching the
// gradient legend shown alongside it in the design.
class HeatmapWidget : public QWidget {
    Q_OBJECT
public:
    explicit HeatmapWidget(QWidget *parent = nullptr);

    void setValues(const std::array<double, 64> &values);
    double maxAbsValue() const { return m_maxAbs; }

    // Overlays each cell's numeric value as text -- backs the design's
    // Heatmap/Numeric toggle without a second, bespoke table widget.
    void setShowValues(bool show);

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize sizeHint() const override;

private:
    std::array<double, 64> m_values{};
    double m_maxAbs = 1.0;
    bool m_showValues = false;
};

} // namespace crankl_gui

#endif // CRANKL_GUI_WIDGETS_HEATMAP_WIDGET_H
