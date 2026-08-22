#include <Workers/WebSocketWorker.h>
#include <UI/MainUI.h>
#include <UI/Dialogs/DialogConnect.h>
#include <Client/Requestor.h>
#include <Client/Extender.h>
#include <Client/Settings.h>
#include <Client/Storage.h>
#include <Client/AuthProfile.h>
#include <Client/ConsoleTheme.h>
#include <Utils/FontManager.h>
#include <Utils/TitleBarStyle.h>
#include <MainAdaptix.h>

#include <QFontInfo>
#include <QTimer>
#include <kddockwidgets/Config.h>
#include <oclero/qlementine.hpp>

MainAdaptix::MainAdaptix()
{
    storage  = new Storage();
    settings = new Settings(this);

    ConsoleThemeManager::instance().loadTheme(settings->data.ConsoleTheme);

    this->SetApplicationTheme();

    mainUI   = new MainUI();
    extender = new Extender(this);

    TitleBarStyle::applyForTheme(mainUI, settings->data.MainTheme);
}

MainAdaptix::~MainAdaptix()
{
    delete mainUI;
    delete extender;
    delete settings;
    delete storage;
}

void MainAdaptix::Start() const
{
    QThread* ChannelThread = nullptr;
    WebSocketWorker* ChannelWsWorker = nullptr;
    AuthProfile* authProfile = nullptr;

    while (true) {
        authProfile = this->Login();
        if (!authProfile) {
            this->Exit();
            return;
        }

        ChannelThread   = new QThread;
        ChannelWsWorker = new WebSocketWorker(authProfile);
        ChannelWsWorker->moveToThread( ChannelThread );

        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);

        connect( ChannelWsWorker, &WebSocketWorker::connected, &loop, &QEventLoop::quit);
        connect( ChannelWsWorker, &WebSocketWorker::ws_error,  &loop, &QEventLoop::quit);
        connect( &timeoutTimer,   &QTimer::timeout,            &loop, &QEventLoop::quit);
        connect( ChannelThread,   &QThread::started, ChannelWsWorker, &WebSocketWorker::run);

        ChannelThread->start();

        timeoutTimer.start(5000);
        loop.exec();

        if (!timeoutTimer.isActive()) {
            MessageError("Server is unreachable");
            if (ChannelThread->isRunning()) {
                ChannelThread->quit();
                ChannelThread->wait();
            }
            delete ChannelWsWorker;
            delete ChannelThread;
            delete authProfile;
            continue;
        }

        timeoutTimer.stop();

        if (!ChannelWsWorker->ok) {
            MessageError(ChannelWsWorker->message);
            if (ChannelThread->isRunning()) {
                ChannelThread->quit();
                ChannelThread->wait();
            }
            delete ChannelWsWorker;
            delete ChannelThread;
            delete authProfile;
            continue;
        }

        break;
    }

    ApplyApplicationFont();

    mainUI->setMinimumSize(500, 500);
    mainUI->resize(1024, 768);
    mainUI->showMaximized();
    mainUI->AddNewProject(authProfile, ChannelThread, ChannelWsWorker);

    QTimer::singleShot(0, [this]() {
        ApplyApplicationFont();
    });

    QApplication::exec();
}

void MainAdaptix::Exit() { QCoreApplication::quit(); }

void MainAdaptix::NewProject() const
{
    QThread* ChannelThread = nullptr;
    WebSocketWorker* ChannelWsWorker = nullptr;
    AuthProfile* authProfile = nullptr;

    while (true) {
        authProfile = this->Login();
        if (!authProfile)
            return;

        ChannelThread   = new QThread;
        ChannelWsWorker = new WebSocketWorker(authProfile);
        ChannelWsWorker->moveToThread( ChannelThread );

        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);

        connect( ChannelWsWorker, &WebSocketWorker::connected, &loop, &QEventLoop::quit);
        connect( ChannelWsWorker, &WebSocketWorker::ws_error,  &loop, &QEventLoop::quit);
        connect( &timeoutTimer,   &QTimer::timeout,            &loop, &QEventLoop::quit);
        connect( ChannelThread,   &QThread::started, ChannelWsWorker, &WebSocketWorker::run);

        ChannelThread->start();

        timeoutTimer.start(5000);
        loop.exec();

        if (!timeoutTimer.isActive()) {
            MessageError("Server is unreachable");
            if (ChannelThread->isRunning()) {
                ChannelThread->quit();
                ChannelThread->wait();
            }

            delete ChannelWsWorker;
            delete ChannelThread;
            delete authProfile;
            continue;
        }

        timeoutTimer.stop();

        if (!ChannelWsWorker->ok) {
            MessageError(ChannelWsWorker->message);
            if (ChannelThread->isRunning()) {
                ChannelThread->quit();
                ChannelThread->wait();
            }
            delete ChannelWsWorker;
            delete ChannelThread;
            delete authProfile;
            continue;
        }

        break;
    }

    mainUI->AddNewProject(authProfile, ChannelThread, ChannelWsWorker);
}

AuthProfile* MainAdaptix::Login()
{
    AuthProfile* authProfile;
    DialogConnect dialogConnect;
    bool result;

    do {
        authProfile = dialogConnect.StartDialog();
        if ( !authProfile || !authProfile->valid)
            return NULL;

        result = HttpReqLogin( authProfile );
        if (!result) {
            if (authProfile->message.isEmpty())
                MessageError("Login failure");
            else
                MessageError(authProfile->message);
            delete authProfile;
        }

    } while( !result );

    return authProfile;
}

void MainAdaptix::SetApplicationTheme()
{
    static bool kddwInitialized = false;
    if (!kddwInitialized) {
        KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtWidgets);
        KDDockWidgets::Config::self().setSeparatorThickness(5);

        auto flags = KDDockWidgets::Config::self().flags();
        flags |= KDDockWidgets::Config::Flag_HideTitleBarWhenTabsVisible;
        flags |= KDDockWidgets::Config::Flag_TabsHaveCloseButton;
        flags |= KDDockWidgets::Config::Flag_ShowButtonsOnTabBarIfTitleBarHidden;
        flags |= KDDockWidgets::Config::Flag_AllowSwitchingTabsViaMenu;
        flags |= KDDockWidgets::Config::Flag_AllowReorderTabs;
        flags |= KDDockWidgets::Config::Flag_DoubleClickMaximizes;
        KDDockWidgets::Config::self().setFlags(flags);
        kddwInitialized = true;
    }

    QGuiApplication::setWindowIcon( QIcon( ":/LogoLin" ) );

    FontManager::instance().initialize();

    auto* style = new oclero::qlementine::QlementineStyle(qApp);
    QString themeName = settings->data.MainTheme;
    QString userPath = QDir(QDir::homePath()).filePath(".adaptix/themes/app/" + themeName + ".json");
    QString themePath = QFile::exists(userPath) ? userPath : QString(":/qlementine-themes/%1").arg(themeName);

    if (!QFile::exists(userPath) && !QFile::exists(themePath)) {
        themeName = "Adaptix_Dark_Emerald";
        settings->data.MainTheme = themeName;
        themePath = QString(":/qlementine-themes/%1").arg(themeName);
    }
    style->setThemeJsonPath(themePath);
    style->setAutoIconColor(oclero::qlementine::AutoIconColor::ForegroundColor);
    qlementineStyle = style;

    ApplyApplicationFont();

    QApplication::setStyle(style);
}

void MainAdaptix::ApplyApplicationFont() const
{
    if (!qlementineStyle)
        return;

    QString appFontFamily = settings->data.FontFamily;
    if (appFontFamily.contains(" - "))
        appFontFamily = appFontFamily.split("-")[1].trimmed();

    appFontFamily = FontManager::instance().resolveFamily(appFontFamily);

    QFont testFont(appFontFamily);
    QFontInfo testInfo(testFont);
    if (testInfo.family() != appFontFamily && !testInfo.family().startsWith(appFontFamily)) {
        appFontFamily = FontManager::instance().resolveFamily("JetBrains Mono");
    }

    int appFontSize = settings->data.FontSize;

    FontManager::instance().rebuildTypography(appFontFamily, appFontSize);

    const AppTypography& ty = FontManager::instance().typography();

    auto theme = qlementineStyle->theme();

    theme.fontRegular = ty.regular;
    theme.fontBold    = ty.bold;

    theme.fontH1.setFamily(appFontFamily);
    theme.fontH2.setFamily(appFontFamily);
    theme.fontH3.setFamily(appFontFamily);
    theme.fontH4.setFamily(appFontFamily);
    theme.fontH5.setFamily(appFontFamily);
    theme.fontH1.setPointSize(appFontSize + 8);
    theme.fontH2.setPointSize(appFontSize + 6);
    theme.fontH3.setPointSize(appFontSize + 4);
    theme.fontH4.setPointSize(appFontSize + 2);
    theme.fontH5.setPointSize(appFontSize + 1);

    theme.fontCaption = ty.caption;

    theme.fontMonospace = ty.mono;

    qlementineStyle->setTheme(theme);
    QApplication::setFont(ty.regular);
}
