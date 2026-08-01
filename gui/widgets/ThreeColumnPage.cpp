#include "widgets/ThreeColumnPage.h"

#include <QHBoxLayout>
#include <QResizeEvent>
#include <QVBoxLayout>

#include <algorithm>

namespace crankl_gui {

namespace {
// Shared with ArchiveHealthHeader's own compact threshold so the header and
// this frame collapse together on InspectPage rather than at two different
// widths.
constexpr int kNarrowWidthThreshold = 900;
} // namespace

ThreeColumnPage::ThreeColumnPage(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("ThreeColumnPage"));
    rebuild();
}

void ThreeColumnPage::setLeftContent(QWidget *widget) {
    m_leftContent = widget;
    // 5c: "Fixed 262 px, never collapses" -- the frame owns the column
    // widths so every page built on it agrees.
    if (m_leftContent)
        m_leftContent->setFixedWidth(262);
    rebuild();
}

void ThreeColumnPage::setCenterContent(QWidget *widget) {
    m_centerContent = widget;
    rebuild();
}

void ThreeColumnPage::setRightContent(QWidget *widget) {
    m_rightContent = widget;
    // 5c: right column fixed at 274 px (when not reflowed below center).
    if (m_rightContent)
        m_rightContent->setFixedWidth(274);
    rebuild();
}

void ThreeColumnPage::setBottomActions(QWidget *widget) {
    m_bottomActions = widget;
    rebuild();
}

void ThreeColumnPage::setAvailableWidth(int width) {
    m_availableWidth = width;
    updateNarrowForWidth(width);
}

void ThreeColumnPage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    // Prefer the page-pushed width; self-measuring is only the fallback for
    // hosts that never call setAvailableWidth().
    updateNarrowForWidth(m_availableWidth > 0 ? m_availableWidth : event->size().width());
}

void ThreeColumnPage::updateNarrowForWidth(int width) {
    int wideFloor = 0;
    if (m_leftContent)
        wideFloor += 262 + 12;
    if (m_centerContent)
        wideFloor += m_centerContent->minimumSizeHint().width();
    if (m_rightContent)
        wideFloor += 12 + 274;
    const bool narrow = width < std::max(kNarrowWidthThreshold, wideFloor + 8);
    if (narrow != m_narrow) {
        m_narrow = narrow;
        rebuild();
    }
}

void ThreeColumnPage::rebuild() {
    // Detach every content widget from whatever it's currently parented
    // under before tearing down the old layout tree
    for (QWidget *w : {m_leftContent, m_centerContent, m_rightContent, m_bottomActions}) {
        if (w)
            w->setParent(nullptr);
    }

    if (QLayout *old = layout()) {
        QLayoutItem *item;
        while ((item = old->takeAt(0)) != nullptr)
            delete item; // never deletes the widget it wraps -- see header comment
        delete old;
    }

    delete m_row;
    delete m_rightColumn;
    delete m_bottomBar;

    // The right column is fixed 274px in the wide layout but spans the full
    // width once it reflows below the center column (3b).
    if (m_rightContent) {
        if (m_narrow) {
            m_rightContent->setMinimumWidth(0);
            m_rightContent->setMaximumWidth(QWIDGETSIZE_MAX);
        } else {
            m_rightContent->setFixedWidth(274);
        }
    }

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);

    m_row = new QWidget(this);
    auto *rowLayout = new QHBoxLayout(m_row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(12);
    if (m_leftContent)
        rowLayout->addWidget(m_leftContent);
    if (m_centerContent)
        rowLayout->addWidget(m_centerContent, 1);

    QWidget *rightColumnParent = m_narrow ? static_cast<QWidget *>(this) : static_cast<QWidget *>(m_row);
    m_rightColumn = new QWidget(rightColumnParent);
    m_rightColumn->setObjectName(QStringLiteral("ThreeColumnRightColumn"));
    auto *rightLayout = new QVBoxLayout(m_rightColumn);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    if (m_rightContent)
        rightLayout->addWidget(m_rightContent, 1);
    if (!m_narrow && m_bottomActions)
        rightLayout->addWidget(m_bottomActions);

    if (!m_narrow)
        rowLayout->addWidget(m_rightColumn);

    root->addWidget(m_row, 1);

    if (m_narrow) {
        root->addWidget(m_rightColumn);
        if (m_bottomActions) {
            m_bottomBar = new QWidget(this);
            m_bottomBar->setObjectName(QStringLiteral("ThreeColumnBottomBar"));
            auto *barLayout = new QHBoxLayout(m_bottomBar);
            barLayout->addWidget(m_bottomActions);
            root->addWidget(m_bottomBar);
        }
    }
}

} // namespace crankl_gui
