#include <Workers/WebSocketWorker.h>
#include <UI/MainUI.h>
#include <UI/Dialogs/DialogConnect.h>
#include <Client/Requestor.h>
#include <Client/Extender.h>
#include <Client/Settings.h>
#include <Client/Storage.h>
#include <Client/AuthProfile.h>
#include <Utils/FontManager.h>
#include <MainAdaptix.h>

#include <kddockwidgets/Config.h>

MainAdaptix::MainAdaptix()
{
    storage  = new Storage();
    settings = new Settings(this);

    this->SetApplicationTheme();

    mainUI   = new MainUI();
    extender = new Extender(this);
}

MainAdaptix::~MainAdaptix()
{
    delete storage;
    delete mainUI;
    delete extender;
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

    mainUI->setMinimumSize(500, 500);
    mainUI->resize(1024, 768);
    mainUI->showMaximized();
    mainUI->AddNewProject(authProfile, ChannelThread, ChannelWsWorker);

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
    auto dialogConnect = new DialogConnect();
    bool result;

    do {
        authProfile = dialogConnect->StartDialog();
        if ( !authProfile || !authProfile->valid)
            return NULL;

        result = HttpReqLogin( authProfile );
        if (!result)
            MessageError("Login failure");

    } while( !result );

    return authProfile;
}

void MainAdaptix::SetApplicationTheme() const
{
    QGuiApplication::setWindowIcon( QIcon( ":/LogoLin" ) );

    KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtWidgets);
    KDDockWidgets::Config::self().setSeparatorThickness(5);

    auto flags = KDDockWidgets::Config::self().flags();
    flags |= KDDockWidgets::Config::Flag_TabsHaveCloseButton;
    flags |= KDDockWidgets::Config::Flag_ShowButtonsOnTabBarIfTitleBarHidden;
    flags |= KDDockWidgets::Config::Flag_AllowSwitchingTabsViaMenu;
    flags |= KDDockWidgets::Config::Flag_AllowReorderTabs;
    flags |= KDDockWidgets::Config::Flag_DoubleClickMaximizes;
    KDDockWidgets::Config::self().setFlags(flags);

    FontManager::instance().initialize();

    QString appFontFamily = settings->data.FontFamily;
    if (appFontFamily.startsWith("Adaptix"))
        appFontFamily = appFontFamily.split("-")[1].trimmed();

    auto appFont = QFont( appFontFamily );
    appFont.setPointSize( settings->data.FontSize );
    QApplication::setFont( appFont );

    QString appTheme = ":/themes/" + settings->data.MainTheme;
    bool result = false;
    QString style = ReadFileString(appTheme, &result);
    if (result) {
        QApplication *app = qobject_cast<QApplication*>(QCoreApplication::instance());
        app->setStyleSheet(style);
    }
}
