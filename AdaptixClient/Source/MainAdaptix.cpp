#include <MainAdaptix.h>
#include <UI/Dialogs/DialogConnect.h>
#include <Client/Requestor.h>

MainAdaptix::MainAdaptix()
{
    storage  = new Storage();

    settings = new Settings(this);

    this->SetApplicationTheme();

    mainUI   = new MainUI;
    extender = new Extender(this);
}

MainAdaptix::~MainAdaptix()
{
    delete storage;
    delete mainUI;
}

void MainAdaptix::Start()
{
    AuthProfile* authProfile;
    auto dialogConnect = new DialogConnect();
    bool result;

    do {
        authProfile = dialogConnect->StartDialog();
        if ( !authProfile || !authProfile->valid) {
            this->Exit();
            return;
        }

        result = HttpReqLogin( authProfile );
        if (!result) {
            MessageError("Login failure");
            continue;
        }

    } while( !result );

    mainUI->showMaximized();
    mainUI->AddNewProject(authProfile);

    QApplication::exec();
}

void MainAdaptix::Exit()
{
    QApplication::exit(0);
}

void MainAdaptix::SetApplicationTheme()
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
