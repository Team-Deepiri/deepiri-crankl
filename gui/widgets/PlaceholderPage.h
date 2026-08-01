#ifndef CRANKL_GUI_WIDGETS_PLACEHOLDER_PAGE_H
#define CRANKL_GUI_WIDGETS_PLACEHOLDER_PAGE_H

#include <QWidget>

namespace crankl_gui {

class PlaceholderPage : public QWidget {
    Q_OBJECT
public:
    explicit PlaceholderPage(const QString &destinationName, QWidget *parent = nullptr);
};

} // namespace crankl_gui

#endif // CRANKL_GUI_WIDGETS_PLACEHOLDER_PAGE_H
