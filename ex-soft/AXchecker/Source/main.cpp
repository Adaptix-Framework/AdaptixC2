#include <main.h>
#include <Classes/MainAX.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    auto window = new MainAX;
    window->Start();
}
