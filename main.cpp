#include <QApplication>
#include <QSystemTrayIcon>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    // Create the Qt application object (sets up the event loop infrastructure).
    QApplication app(argc, argv);

    app.setApplicationName("WinAgent Dashboard");
    app.setApplicationVersion("0.2.0");
    app.setWindowIcon(QIcon("app_icon.ico"));
    app.setQuitOnLastWindowClosed(false);

    // Create and show the main window (the dashboard UI).
    MainWindow w;
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        w.show();
    } else {
        w.hide();
    }

    // Start the Qt event loop. This call blocks until the app exits.
    return app.exec();
}