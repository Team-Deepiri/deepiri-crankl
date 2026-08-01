#ifndef CRANKL_GUI_WIDGETS_STYLE_UTIL_H
#define CRANKL_GUI_WIDGETS_STYLE_UTIL_H

#include <QFrame>
#include <QScrollArea>
#include <QStyle>
#include <QVariant>
#include <QWidget>

namespace crankl_gui {

inline void setStyleProperty(QWidget *w, const char *name, const QVariant &value) {
    w->setProperty(name, value);
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

// A QSS-stylable card container. Plain QWidget ignores stylesheet
// background/border unless WA_StyledBackground is set -- QFrame plus the
// attribute renders both reliably.
inline QFrame *makeCard(const QString &objectName, QWidget *parent = nullptr) {
    auto *frame = new QFrame(parent);
    frame->setObjectName(objectName);
    frame->setAttribute(Qt::WA_StyledBackground, true);
    return frame;
}

// Wraps a page's content in a vertical-only scroll area so the window can
// shrink below the content's natural height on small screens (1280x720)
inline QScrollArea *makeVScroll(QWidget *content, QWidget *parent = nullptr) {
    auto *scroll = new QScrollArea(parent);
    scroll->setObjectName(QStringLiteral("PageScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->viewport()->setAutoFillBackground(false);
    scroll->setWidget(content);
    return scroll;
}

} // namespace crankl_gui

#endif // CRANKL_GUI_WIDGETS_STYLE_UTIL_H
