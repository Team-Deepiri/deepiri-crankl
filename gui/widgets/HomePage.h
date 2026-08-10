#ifndef CRANKL_GUI_WIDGETS_HOME_PAGE_H
#define CRANKL_GUI_WIDGETS_HOME_PAGE_H

#include "core/ArchiveSnapshot.h"

#include <QWidget>

class QLabel;
class QStackedWidget;
class QDragEnterEvent;
class QDropEvent;
class QResizeEvent;

namespace crankl_gui {

class ArchiveHealthHeader;

class HomePage : public QWidget {
    Q_OBJECT
  public:
    explicit HomePage(QWidget *parent = nullptr);

    void showEmptyState();
    void showArchive(const ArchiveSnapshot &snapshot);
    void showUnsupportedFile(const QString &fileName);

  Q_SIGNALS:
    void openArchiveRequested();
    void openWeightsRequested();
    void pathDropped(QString path);
    void inspectRequested();
    void reVerifyRequested();
    void closeArchiveRequested();

  protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

  private:
    QWidget *buildEmptyStatePanel();
    QWidget *buildSelectedStatePanel();
    QWidget *buildUnsupportedPanel();
    QWidget *buildActionButtonsGrid();

    QStackedWidget *m_stack = nullptr;
    QWidget *m_emptyPanel = nullptr;
    QWidget *m_selectedPanel = nullptr;
    QWidget *m_unsupportedPanel = nullptr;

    ArchiveHealthHeader *m_overviewHeader = nullptr;
    QLabel *m_identityModel = nullptr;
    QLabel *m_identityHash = nullptr;
    QLabel *m_lifecycleLog = nullptr;
    QLabel *m_unsupportedFileName = nullptr;
};

} // namespace crankl_gui

#endif // CRANKL_GUI_WIDGETS_HOME_PAGE_H
