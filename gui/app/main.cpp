#include "widgets/MainWindow.h"

#include <QApplication>
#include <QDebug>
#include <QFile>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Deepiri"));
    QApplication::setApplicationName(QStringLiteral("crankl"));

    // Non-fatal: the app is fully usable in Qt's default style, so a missing
    // or unreadable theme warns and continues rather than blocking startup.
    // It is compiled into the binary via AUTORCC, so failing here means the
    // resource was not linked in -- worth surfacing to whoever is debugging it.
    QFile themeFile(QStringLiteral(":/theme.qss"));
    if (themeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(themeFile.readAll()));
    } else {
        qWarning("crankl-gui: could not load :/theme.qss (%s) -- falling back to the default style",
                 qUtf8Printable(themeFile.errorString()));
    }

    crankl_gui::MainWindow window;
    window.show();

    return QApplication::exec();
}
