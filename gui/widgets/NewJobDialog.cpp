#include "widgets/NewJobDialog.h"

#include "core/CliCommands.h"
#include "widgets/StyleUtil.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <utility>

namespace crankl_gui {

namespace {

QLineEdit *makePathEdit(QWidget *parent) {
    auto *edit = new QLineEdit(parent);
    edit->setPlaceholderText(QObject::tr("Path…"));
    edit->setMinimumWidth(320);
    return edit;
}

QPushButton *makeBrowseButton(QWidget *parent) {
    auto *button = new QPushButton(QObject::tr("Browse…"), parent);
    button->setObjectName(QStringLiteral("SecondaryButton"));
    return button;
}

} // namespace

NewJobDialog::NewJobDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("New crankl job"));
    setModal(true);
    setObjectName(QStringLiteral("NewJobDialog"));
    setMinimumWidth(560);

    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(tr("Pack weights"), static_cast<int>(JobType::Pack));
    m_typeCombo->addItem(tr("Turn (one-step optimize)"), static_cast<int>(JobType::Turn));
    m_typeCombo->addItem(tr("Finetune"), static_cast<int>(JobType::Finetune));
    m_typeCombo->addItem(tr("Peel (rollback layers)"), static_cast<int>(JobType::Peel));
    form->addRow(tr("Operation"), m_typeCombo);

    m_inputEdit = makePathEdit(this);
    auto *inputRow = new QHBoxLayout;
    auto *browseInput = makeBrowseButton(this);
    inputRow->addWidget(m_inputEdit, 1);
    inputRow->addWidget(browseInput);
    form->addRow(tr("Input"), inputRow);

    m_outputEdit = makePathEdit(this);
    auto *outputRow = new QHBoxLayout;
    auto *browseOutput = makeBrowseButton(this);
    outputRow->addWidget(m_outputEdit, 1);
    outputRow->addWidget(browseOutput);
    form->addRow(tr("Output"), outputRow);

    m_stepsSpin = new QSpinBox(this);
    m_stepsSpin->setRange(1, 4096);
    m_stepsSpin->setValue(1);
    form->addRow(tr("Steps"), m_stepsSpin);

    m_lrSpin = new QDoubleSpinBox(this);
    m_lrSpin->setRange(1e-6, 1.0);
    m_lrSpin->setDecimals(6);
    m_lrSpin->setValue(0.001);
    form->addRow(tr("Learning rate"), m_lrSpin);

    m_layersSpin = new QSpinBox(this);
    m_layersSpin->setRange(1, 64);
    m_layersSpin->setValue(1);
    form->addRow(tr("Layers to pop"), m_layersSpin);

    layout->addLayout(form);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setObjectName(QStringLiteral("CommandPreview"));
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_previewLabel);

    auto *buttonRow = new QHBoxLayout;
    auto *cancelButton = new QPushButton(tr("Cancel"), this);
    cancelButton->setObjectName(QStringLiteral("SecondaryButton"));
    m_runButton = new QPushButton(tr("Run"), this);
    m_runButton->setObjectName(QStringLiteral("PrimaryButton"));
    buttonRow->addWidget(cancelButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(m_runButton);
    layout->addLayout(buttonRow);

    connect(m_typeCombo, &QComboBox::currentIndexChanged, this, [this] { onTypeChanged(); });
    connect(browseInput, &QPushButton::clicked, this, &NewJobDialog::browseInput);
    connect(browseOutput, &QPushButton::clicked, this, &NewJobDialog::browseOutput);
    connect(m_inputEdit, &QLineEdit::textChanged, this, [this] { updatePreview(); });
    connect(m_outputEdit, &QLineEdit::textChanged, this, [this] { updatePreview(); });
    connect(m_stepsSpin, &QSpinBox::valueChanged, this, [this] { updatePreview(); });
    connect(m_lrSpin, &QDoubleSpinBox::valueChanged, this, [this] { updatePreview(); });
    connect(m_layersSpin, &QSpinBox::valueChanged, this, [this] { updatePreview(); });
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_runButton, &QPushButton::clicked, this, &NewJobDialog::accept);

    onTypeChanged();
}

void NewJobDialog::setInputPath(const QString &path) {
    m_lastInput = path;
    m_inputEdit->setText(path);
    m_outputEdit->setText(suggestOutput());
    updatePreview();
}

QString NewJobDialog::inputPath() const {
    return m_inputEdit->text();
}

void NewJobDialog::onTypeChanged() {
    const bool needsSteps =
        m_typeCombo->currentIndex() == 1 || m_typeCombo->currentIndex() == 2; // Turn | Finetune
    const bool needsLayers = m_typeCombo->currentIndex() == 3;                // Peel
    m_stepsSpin->setVisible(needsSteps);
    m_lrSpin->setVisible(needsSteps);
    m_layersSpin->setVisible(needsLayers);
    m_outputEdit->setText(suggestOutput());
    updatePreview();
}

void NewJobDialog::browseInput() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select input"), QFileInfo(m_inputEdit->text()).absolutePath(),
        tr("Crank archives (*.crank);;Raw float32 (*.f32)"));
    if (!path.isEmpty()) {
        m_lastInput = path;
        m_inputEdit->setText(path);
        m_outputEdit->setText(suggestOutput());
        updatePreview();
    }
}

void NewJobDialog::browseOutput() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Select output"), m_outputEdit->text(), tr("Crank archives (*.crank)"));
    if (!path.isEmpty())
        m_outputEdit->setText(path);
}

QString NewJobDialog::suggestOutput() const {
    const QString input = m_inputEdit->text().isEmpty() ? m_lastInput : m_inputEdit->text();
    if (input.isEmpty())
        return QString();
    const QFileInfo info(input);
    return info.dir().filePath(info.completeBaseName() + QStringLiteral("_out.crank"));
}

void NewJobDialog::updatePreview() {
    const JobType type = static_cast<JobType>(m_typeCombo->currentData().toInt());
    QStringList args;
    const QString input = m_inputEdit->text().trimmed();
    const QString output = m_outputEdit->text().trimmed();
    switch (type) {
    case JobType::Pack:
        args = packArgs(input, output);
        break;
    case JobType::Turn:
        args = turnArgs(input, output, m_stepsSpin->value(), m_lrSpin->value());
        break;
    case JobType::Finetune:
        args = finetuneArgs(input, output, m_stepsSpin->value(), m_lrSpin->value());
        break;
    case JobType::Peel:
        args = peelArgs(input, output, m_layersSpin->value());
        break;
    default:
        break;
    }
    m_previewLabel->setText(
        QStringLiteral("crankl %1 %2").arg(cliCommandName(type), args.join(QLatin1Char(' '))));
    const bool ready = !input.isEmpty() && !output.isEmpty();
    m_runButton->setEnabled(ready);
}

void NewJobDialog::accept() {
    const JobType type = static_cast<JobType>(m_typeCombo->currentData().toInt());
    const QString input = m_inputEdit->text().trimmed();
    const QString output = m_outputEdit->text().trimmed();
    if (input.isEmpty() || output.isEmpty())
        return;

    QStringList args;
    switch (type) {
    case JobType::Pack:
        args = packArgs(input, output);
        break;
    case JobType::Turn:
        args = turnArgs(input, output, m_stepsSpin->value(), m_lrSpin->value());
        break;
    case JobType::Finetune:
        args = finetuneArgs(input, output, m_stepsSpin->value(), m_lrSpin->value());
        break;
    case JobType::Peel:
        args = peelArgs(input, output, m_layersSpin->value());
        break;
    default:
        return;
    }

    const QFileInfo outInfo(output);
    const QString workingDir = outInfo.dir().absolutePath();
    Q_EMIT jobRequested(
        type, QStringLiteral("%1 · %2").arg(cliCommandName(type), QFileInfo(input).fileName()),
        input, args, workingDir);
    QDialog::accept();
}

} // namespace crankl_gui