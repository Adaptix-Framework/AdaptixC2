#include <main.h>
#include <MainAdaptix.h>

MainAdaptix* GlobalClient = nullptr;

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    GlobalClient = new MainAdaptix;
    GlobalClient->Start();
}
