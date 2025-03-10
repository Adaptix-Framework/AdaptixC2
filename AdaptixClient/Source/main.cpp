#include <main.h>
#include <MainAdaptix.h>

MainAdaptix* GlobalClient = nullptr;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setQuitOnLastWindowClosed(true);

    GlobalClient = new MainAdaptix();
    GlobalClient->Start();
}
