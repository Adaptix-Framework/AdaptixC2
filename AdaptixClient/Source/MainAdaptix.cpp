#include <MainAdaptix.h>
#include <UI/Dialogs/DialogConnect.h>
#include <Client/Requestor.h>

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
    AuthProfile* authProfile = this->Login();
    if (!authProfile) {
        this->Exit();
        return;
    }

    mainUI->showMaximized();
    mainUI->AddNewProject(authProfile);

    QApplication::exec();
}

void MainAdaptix::Exit()
{
    QCoreApplication::quit();
}

void MainAdaptix::NewProject() const
{
    AuthProfile* authProfile = this->Login();
    if (authProfile)
        mainUI->AddNewProject(authProfile);
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

    QFontDatabase::addApplicationFont(":/fonts/DroidSansMono");
    QFontDatabase::addApplicationFont(":/fonts/Hack");
    QFontDatabase::addApplicationFont(":/fonts/DejavuSansMono");

    QString appFontFamily = settings->data.FontFamily;
    if (appFontFamily.startsWith("Adaptix")) {
        appFontFamily = appFontFamily.split("-")[1].trimmed();
    }

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
