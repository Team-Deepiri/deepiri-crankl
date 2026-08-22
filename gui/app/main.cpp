#include "widgets/MainWindow.h"

#include <QApplication>
#include <QDebug>
#include <QFile>

namespace {

// --smoke-test: bring up the full UI and exit 0 immediately. Lets CI / shell
// scripts verify the binary links, its resources load, and every page builds,
// without needing a display (QT_QPA_PLATFORM=offscreen) or a timed kill.
bool handleSmokeTest(const QStringList &args) {
    return args.contains(QStringLiteral("--smoke-test"));
}

} // namespace

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

    if (handleSmokeTest(app.arguments())) {
        crankl_gui::MainWindow window;
        window.show();
        return 0;
    }

    crankl_gui::MainWindow window;
    window.show();

    return QApplication::exec();
}
