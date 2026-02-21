#include <Workers/WebSocketWorker.h>
#include <UI/MainUI.h>
#include <UI/Dialogs/DialogConnect.h>
#include <Client/Requestor.h>
#include <Client/Extender.h>
#include <Client/Settings.h>
#include <Client/Storage.h>
#include <Client/AuthProfile.h>
#include <Utils/FontManager.h>
#include <Utils/TitleBarStyle.h>
#include <MainAdaptix.h>

#include <kddockwidgets/Config.h>
#include <oclero/qlementine.hpp>

MainAdaptix::MainAdaptix()
{
    storage  = new Storage();
    settings = new Settings(this);

    this->SetApplicationTheme();

    mainUI   = new MainUI();
    extender = new Extender(this);

    TitleBarStyle::applyForTheme(mainUI, settings->data.MainTheme);
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

    auto* style = new oclero::qlementine::QlementineStyle(qApp);
    QString themePath = QString(":/qlementine-themes/%1").arg(settings->data.MainTheme);
    style->setThemeJsonPath(themePath);
    const_cast<MainAdaptix*>(this)->qlementineStyle = style;
    QApplication::setStyle(style);

    FontManager::instance().initialize();

    QString appFontFamily = settings->data.FontFamily;
    if (appFontFamily.startsWith("Adaptix"))
        appFontFamily = appFontFamily.split("-")[1].trimmed();

    auto appFont = QFont( appFontFamily );
    appFont.setPointSize( settings->data.FontSize );
    QApplication::setFont( appFont );

    QString additionalStyles = R"(
        QLabel[LabelStyle="console"] {
            padding: 4px;
            color: #BEBEBE;
            background-color: transparent;
        }
        QLineEdit[LineEditStyle="console"] {
            background-color: #151515;
            color: #BEBEBE;
            border: 1px solid #2A2A2A;
            padding: 4px;
            border-radius: 4px;
        }
        QLineEdit[LineEditStyle="console"]:focus {
            border: 1px solid #3D8B6A;
        }
        QTextEdit[TextEditStyle="console"], QPlainTextEdit[TextEditStyle="console"] {
            background-color: #151515;
            color: #BEBEBE;
            border: 1px solid #2A2A2A;
            border-radius: 4px;
        }

        QMenu::separator {
            height: 1px;
            background-color: #3A3A3A;
            margin: 4px 8px;
        }
    )";
    QApplication *app = qobject_cast<QApplication*>(QCoreApplication::instance());
    app->setStyleSheet(additionalStyles);
}
