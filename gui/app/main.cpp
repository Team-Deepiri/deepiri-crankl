#include "widgets/MainWindow.h"

#include <QApplication>
#include <QFile>
#include <QTimer>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Deepiri"));
    QApplication::setApplicationName(QStringLiteral("crankl"));

    QFile themeFile(QStringLiteral(":/theme.qss"));
    if (themeFile.open(QIODevice::ReadOnly | QIODevice::Text))
        app.setStyleSheet(QString::fromUtf8(themeFile.readAll()));

    crankl_gui::MainWindow window;
    window.show();

    return QApplication::exec();
}
