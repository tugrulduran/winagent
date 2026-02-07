#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    // Qt Uygulamasını başlat
    QApplication app(argc, argv);

    // Ana pencereyi oluştur ve göster
    MainWindow w;
    w.show();

    // Uygulama döngüsüne gir (Event Loop)
    return app.exec();
}