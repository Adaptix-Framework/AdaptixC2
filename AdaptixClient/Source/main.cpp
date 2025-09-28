#include <main.h>
#include <MainAdaptix.h>

#include <QLoggingCategory>

MainAdaptix* GlobalClient = nullptr;

int main(int argc, char *argv[])
{
    QLoggingCategory::setFilterRules(
        "qt.text.font.db=false\n"
        "qt.text.font.db.debug=false\n"
        "qt.text.font.db.warning=false\n"
        "qt.text.font.db.info=false\n"
        "qt.text.font.db.critical=false"
    );

    QApplication a(argc, argv);

    a.setQuitOnLastWindowClosed(true);

    GlobalClient = new MainAdaptix();
    GlobalClient->Start();
}
