#include <main.h>
#include <Classes/MainCmd.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    auto window = new MainCmd;
    window->Start();
}