#include <MainAdaptix.h>
#include <UI/Dialogs/DialogConnect.h>
#include <Client/Requestor.h>

MainAdaptix::MainAdaptix()
{
    storage  = new Storage();

    this->SetApplicationTheme();

    mainUI   = new MainUI;
    extender = new Extender(this);
}

MainAdaptix::~MainAdaptix()
{
    delete storage;
    delete mainUI;
};

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

    QFontDatabase::addApplicationFont( ":/fonts/DroidSansMono" );
    QFontDatabase::addApplicationFont( ":/fonts/Hack" );
    int FontID = QFontDatabase::addApplicationFont( ":/fonts/DejavuSansMono" );
    QString FontFamily = QFontDatabase::applicationFontFamilies( FontID ).at( 0 );
    auto Font = QFont( FontFamily );

    Font.setPointSize( 10 );
    QApplication::setFont( Font );

    QString theme = ":/themes/Dark";

    bool result = false;
    QString style = ReadFileString(theme, &result);
    if (result) {
        QApplication *app = qobject_cast<QApplication*>(QCoreApplication::instance());
        app->setStyleSheet(style);
    }
}
