#ifndef CRANKL_GUI_WIDGETS_INSPECT_PAGE_H
#define CRANKL_GUI_WIDGETS_INSPECT_PAGE_H

#include "core/ArchiveSnapshot.h"

#include <QJsonObject>
#include <QWidget>

class QLabel;
class QSpinBox;
class QSlider;
class QCheckBox;
class QComboBox;
class QFrame;
class QPushButton;
class QDragEnterEvent;
class QDropEvent;
class QResizeEvent;

namespace crankl_gui {

class ArchiveHealthHeader;
class SlotBrowser;
class ResultSummaryPanel;
class ThreeColumnPage;

// Inspect / Stats / Verify -- the one-page read-only audit on the design's
// canonical three-column frame
class InspectPage : public QWidget {
    Q_OBJECT
public:
    explicit InspectPage(QWidget *parent = nullptr);

    void setSnapshot(const ArchiveSnapshot &snapshot);
    void clearSnapshot();
    void debugDump();

Q_SIGNALS:
    void verifyRequested();
    void closeRequested();
    void changeArchiveRequested(); // Open archive… / Change — same file dialog
    void recentRequested();
    void pathDropped(QString path);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QWidget *buildLeftColumn();
    QJsonObject buildReportJson() const;
    void applyEmptyState(bool empty);
    void copyAsJson();
    void exportStatsReport();

    ArchiveHealthHeader *m_header = nullptr;
    ThreeColumnPage *m_frame = nullptr;
    SlotBrowser *m_slotBrowser = nullptr;
    ResultSummaryPanel *m_resultSummary = nullptr;

    QFrame *m_inputCard = nullptr;
    QLabel *m_inputHeading = nullptr;
    QFrame *m_pathBox = nullptr;
    QLabel *m_pathLabel = nullptr;
    QPushButton *m_changeButton = nullptr;
    QPushButton *m_openButton = nullptr;

    QFrame *m_paramsCard = nullptr;
    QLabel *m_paramsStatus = nullptr;
    QSpinBox *m_slotSpin = nullptr;
    QSlider *m_slotSlider = nullptr;
    QPushButton *m_heatmapButton = nullptr;
    QPushButton *m_numericButton = nullptr;
    QComboBox *m_wordDisplayCombo = nullptr;
    QCheckBox *m_reVerifyOnOpenCheck = nullptr;
    QLabel *m_paramsFooter = nullptr;

    ArchiveSnapshot m_snapshot;
};

} // namespace crankl_gui

#endif // CRANKL_GUI_WIDGETS_INSPECT_PAGE_H
