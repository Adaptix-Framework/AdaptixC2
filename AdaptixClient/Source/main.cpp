#include <main.h>
#include <MainAdaptix.h>

#include <QLoggingCategory>
#include <QSslSocket>

MainAdaptix* GlobalClient = nullptr;

static QtMessageHandler defaultHandler = nullptr;

void messageFilter(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Filter out known Qt warnings during cleanup
    if (msg.contains("invalid nullptr parameter"))
        return;
    if (msg.contains("Creating a fake screen"))
        return;
    
    if (defaultHandler)
        defaultHandler(type, context, msg);
}

int main(int argc, char *argv[])
{
    defaultHandler = qInstallMessageHandler(messageFilter);
    
    QLoggingCategory::setFilterRules(
        "qt.text.font.db=false\n"
        "qt.text.font.db.debug=false\n"
        "qt.text.font.db.warning=false\n"
        "qt.text.font.db.info=false\n"
        "qt.text.font.db.critical=false\n"
        "qt.core.qobject.connect=false"
    );

    QApplication a(argc, argv);

    // Force early SSL backend initialization
    QSslSocket::supportsSsl();

    a.setQuitOnLastWindowClosed(true);

    GlobalClient = new MainAdaptix();
    GlobalClient->Start();
}
