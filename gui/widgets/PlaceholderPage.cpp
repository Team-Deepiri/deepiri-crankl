#include "widgets/PlaceholderPage.h"

#include <QLabel>
#include <QVBoxLayout>

namespace crankl_gui {

PlaceholderPage::PlaceholderPage(const QString &destinationName, QWidget *parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("PlaceholderPage"));

    auto *layout = new QVBoxLayout(this);
    layout->addStretch(1);

    auto *title = new QLabel(destinationName, this);
    title->setObjectName(QStringLiteral("PlaceholderTitle"));
    title->setAlignment(Qt::AlignCenter);

    auto *subtitle = new QLabel(tr("Not available yet."), this);
    subtitle->setObjectName(QStringLiteral("PlaceholderSubtitle"));
    subtitle->setAlignment(Qt::AlignCenter);

    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addStretch(1);
}

} // namespace crankl_gui
