#include <main.h>
#include <Classes/MainAX.h>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    MainAX window;
    window.setWindowTitle("AX Checker");
    window.resize(600, 400);
    window.show();

    return app.exec();
}
