#include <main.h>
#include <MainAdaptix.h>

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QSslSocket>

MainAdaptix* GlobalClient = nullptr;

static QtMessageHandler defaultHandler = nullptr;

void messageFilter(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (msg.contains("invalid nullptr parameter"))
        return;
    if (msg.contains("Creating a fake screen"))
        return;
    if (msg.contains("mapTo(): parent must be in parent hierarchy"))
        return;
    if (msg.contains("wildcard call disconnects from destroyed signal"))
        return;

    if (defaultHandler)
        defaultHandler(type, context, msg);
}

static void applyLinuxQpaDefaults()
{
#if defined(Q_OS_LINUX)
    qunsetenv("QT_QPA_PLATFORMTHEME");
    qunsetenv("QT_STYLE_OVERRIDE");
    QGuiApplication::setDesktopSettingsAware(false);

    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "xcb");
#endif
}

int main(int argc, char *argv[])
{
    applyLinuxQpaDefaults();

    defaultHandler = qInstallMessageHandler(messageFilter);
    
    QLoggingCategory::setFilterRules(
        "qt.text.font.db=false\n"
        "qt.text.font.db.debug=false\n"
        "qt.text.font.db.warning=false\n"
        "qt.text.font.db.info=false\n"
        "qt.text.font.db.critical=false\n"
        "qt.core.qobject.connect=false\n"
        "kf.kio.widgets.kdirmodel=false"
    );

    QApplication a(argc, argv);

    // Force early SSL backend initialization
    QSslSocket::supportsSsl();

    a.setQuitOnLastWindowClosed(true);

    GlobalClient = new MainAdaptix();
    GlobalClient->Start();

    delete GlobalClient;
    GlobalClient = nullptr;

    return 0;
}
