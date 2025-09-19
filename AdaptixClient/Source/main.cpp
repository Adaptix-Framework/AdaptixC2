#include <main.h>
#include <MainAdaptix.h>
#include <Utils/QtFontFix.h>

MainAdaptix* GlobalClient = nullptr;

int main(int argc, char *argv[])
{
    // 在创建QApplication之前初始化字体修复
    QtFontFix::initialize();
    
    QApplication a(argc, argv);

    a.setQuitOnLastWindowClosed(true);

    GlobalClient = new MainAdaptix();
    GlobalClient->Start();
}
