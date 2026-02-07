#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    // Create the Qt application object (sets up the event loop infrastructure).
    QApplication app(argc, argv);

    // Create and show the main window (the dashboard UI).
    MainWindow w;
    w.show();

    // Start the Qt event loop. This call blocks until the app exits.
    return app.exec();
}