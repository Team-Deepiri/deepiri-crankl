#ifndef CRANKL_GUI_WIDGETS_COMPARE_PAGE_H
#define CRANKL_GUI_WIDGETS_COMPARE_PAGE_H

#include "core/CompareResult.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QGridLayout;
class QDragEnterEvent;
class QDropEvent;

namespace crankl_gui {

// §11: structural + numerical comparison of two .crank archives. Inputs on
// top, a summary strip, then a metric delta table and the per-slot changed
// list. Comparison runs through the C API on the worker (JobManager); this
// page only displays the CompareResult it is handed.
class ComparePage : public QWidget {
    Q_OBJECT
  public:
    explicit ComparePage(QWidget *parent = nullptr);

    // Prefills Archive A (e.g. from the currently open archive) and switches
    // focus to the B slot.
    void setArchiveA(const QString &path);
    void showResult(const CompareResult &result);
    void setBusy(bool busy);
    void clear();

  Q_SIGNALS:
    void compareRequested(QString archiveA, QString archiveB);

  protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

  private:
    void onCompareClicked();
    void updateSummary(const CompareResult &result);
    void populateDeltaTable(const CompareResult &result);
    void populateChangedList(const CompareResult &result);
    void copyResultJson(const CompareResult &result);
    void exportResultJson(const CompareResult &result);
    QGridLayout *buildSummaryStrip();
    bool pathsReady() const;

    QLineEdit *m_pathA = nullptr;
    QLineEdit *m_pathB = nullptr;
    QPushButton *m_compareButton = nullptr;

    QGridLayout *m_summaryGrid = nullptr; // label -> value cards, rebuilt per result
    QTableWidget *m_deltaTable = nullptr;
    QTableWidget *m_changedTable = nullptr;
    QLabel *m_changedCaption = nullptr;
    QPushButton *m_copyJsonButton = nullptr;
    QPushButton *m_exportJsonButton = nullptr;

    CompareResult m_result;
};

} // namespace crankl_gui

#endif // CRANKL_GUI_WIDGETS_COMPARE_PAGE_H