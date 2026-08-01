#ifndef CRANKL_GUI_WIDGETS_THREE_COLUMN_PAGE_H
#define CRANKL_GUI_WIDGETS_THREE_COLUMN_PAGE_H

#include <QPointer>
#include <QWidget>

class QResizeEvent;

namespace crankl_gui {

// The reusable app-wide operation-page frame (§3.3, corrected by the
// design's Turn 3): left input/parameters column (never collapses), center
// preview column, right result column with a swappable bottom-action slot.
//
// Later phases (Pack, Optimize, ...) reuse this exact frame
class ThreeColumnPage : public QWidget {
    Q_OBJECT
public:
    explicit ThreeColumnPage(QWidget *parent = nullptr);

    void setLeftContent(QWidget *widget);
    void setCenterContent(QWidget *widget);
    void setRightContent(QWidget *widget);
    void setBottomActions(QWidget *widget);

    void setAvailableWidth(int width);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuild();
    void updateNarrowForWidth(int width);

    int m_availableWidth = -1;

    QWidget *m_leftContent = nullptr;
    QWidget *m_centerContent = nullptr;
    QWidget *m_rightContent = nullptr;
    QWidget *m_bottomActions = nullptr;
    bool m_narrow = false;

    // Transient wrapper widgets rebuild() creates fresh each time. Removing
    // a widget from a QLayout does not delete it (QLayoutItem::~QLayoutItem
    // never deletes the widget it manages) -- these are tracked explicitly
    // so the previous rebuild's wrappers are deleted, not just orphaned as
    // invisible children accumulating under `this`.
    //
    // QPointer, not QWidget*: m_rightColumn's parent is `this` when narrow
    // but `m_row` when wide (see rebuild()), so deleting m_row can cascade-
    // delete m_rightColumn as a side effect through Qt's normal parent-
    // child cleanup. A raw pointer would then dangle and a subsequent
    // `delete m_rightColumn` would be a use-after-free; QPointer is
    // automatically nulled the instant the object it tracks is destroyed,
    // by any means, so the same `delete` becomes a safe no-op.
    QPointer<QWidget> m_row;
    QPointer<QWidget> m_rightColumn;
    QPointer<QWidget> m_bottomBar;
};

} // namespace crankl_gui

#endif // CRANKL_GUI_WIDGETS_THREE_COLUMN_PAGE_H
