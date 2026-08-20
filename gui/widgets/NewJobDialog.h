#ifndef CRANKL_GUI_WIDGETS_NEW_JOB_DIALOG_H
#define CRANKL_GUI_WIDGETS_NEW_JOB_DIALOG_H

#include "core/CranklJob.h"

#include <QDialog>
#include <QStringList>

class QComboBox;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace crankl_gui {

// §16 Jobs: dialog for launching a real `crankl <command>` subprocess job
// (pack / turn / finetune / peel). Compare has its own two-archive page. The
// dialog builds the CLI tokens after <command> and emits them; JobManager
// resolves the executable, queues, runs and cancels the QProcess.
class NewJobDialog : public QDialog {
    Q_OBJECT
  public:
    explicit NewJobDialog(QWidget *parent = nullptr);

    // Prefills the primary input (e.g. the currently open archive) and
    // derives a suggested output path.
    void setInputPath(const QString &path);
    QString inputPath() const;

  Q_SIGNALS:
    void jobRequested(JobType type, const QString &operationLabel, const QString &targetPath,
                      const QStringList &args, const QString &workingDirectory);

  private Q_SLOTS:
    void onTypeChanged();
    void browseInput();
    void browseOutput();
    void accept() override;

  private:
    QString suggestOutput() const;
    void updatePreview();

    QComboBox *m_typeCombo = nullptr;
    QLineEdit *m_inputEdit = nullptr;
    QLineEdit *m_outputEdit = nullptr;
    QSpinBox *m_stepsSpin = nullptr;
    QDoubleSpinBox *m_lrSpin = nullptr;
    QSpinBox *m_layersSpin = nullptr;
    QLabel *m_previewLabel = nullptr;
    QPushButton *m_runButton = nullptr;

    QString m_lastInput; // for suggestOutput() when the field is cleared
};

} // namespace crankl_gui

#endif // CRANKL_GUI_WIDGETS_NEW_JOB_DIALOG_H