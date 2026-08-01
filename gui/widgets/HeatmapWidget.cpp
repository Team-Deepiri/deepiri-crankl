#include "widgets/HeatmapWidget.h"

#include <QPainter>
#include <QPaintEvent>

#include <algorithm>
#include <cmath>

namespace crankl_gui {

namespace {

QColor lerp(const QColor &a, const QColor &b, double t) {
    return QColor(static_cast<int>(std::lround(a.red() + (b.red() - a.red()) * t)),
                  static_cast<int>(std::lround(a.green() + (b.green() - a.green()) * t)),
                  static_cast<int>(std::lround(a.blue() + (b.blue() - a.blue()) * t)));
}

// Orange (negative) / dark-neutral (zero) / cyan (positive), approximating
// the design's oklch gradient legend in plain RGB.
QColor heatColor(double normalized) {
    normalized = std::clamp(normalized, -1.0, 1.0);
    static const QColor kNegative(232, 147, 90);
    static const QColor kNeutral(36, 42, 46);
    static const QColor kPositive(94, 203, 224);
    return normalized < 0.0 ? lerp(kNeutral, kNegative, -normalized)
                             : lerp(kNeutral, kPositive, normalized);
}

} // namespace

HeatmapWidget::HeatmapWidget(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("HeatmapWidget"));
}

void HeatmapWidget::setValues(const std::array<double, 64> &values) {
    m_values = values;
    double maxAbs = 0.0;
    for (double v : m_values)
        maxAbs = std::max(maxAbs, std::fabs(v));
    m_maxAbs = std::max(maxAbs, 1e-6);
    update();
}

void HeatmapWidget::setShowValues(bool show) {
    if (m_showValues == show)
        return;
    m_showValues = show;
    update();
}

void HeatmapWidget::paintEvent(QPaintEvent * /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    constexpr int kGap = 3;
    const double cell = (std::min(width(), height()) - kGap * 7) / 8.0;
    if (cell <= 0.0)
        return;

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const double value = m_values[static_cast<size_t>(row * 8 + col)];
            const double normalized = value / m_maxAbs;
            painter.setPen(Qt::NoPen);
            painter.setBrush(heatColor(normalized));
            const QRectF rect(col * (cell + kGap), row * (cell + kGap), cell, cell);
            painter.drawRoundedRect(rect, 2.0, 2.0);

            if (m_showValues && cell >= 24.0) {
                painter.setPen(normalized > -0.15 && normalized < 0.15 ? Qt::white
                                                                        : QColor(20, 20, 20));
                QFont font = painter.font();
                font.setPointSizeF(std::max(6.0, cell / 6.0));
                painter.setFont(font);
                painter.drawText(rect, Qt::AlignCenter, QString::number(value, 'f', 2));
            }
        }
    }
}

QSize HeatmapWidget::sizeHint() const { return QSize(212, 212); }

} // namespace crankl_gui
