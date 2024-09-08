#include <MainAdaptix.h>
#include <UI/Dialogs/DialogConnect.h>
#include <Client/Requestor.h>

MainAdaptix::MainAdaptix() {
    storage = new Storage();
    this->SetApplicationTheme();
}

MainAdaptix::~MainAdaptix(){
    delete storage;
    delete mainUI;
};

void MainAdaptix::Start() {
    auto dialogConnect = new DialogConnect();
    auto authProfile = dialogConnect->StartDialog();
    if ( ! authProfile.valid ) {
        this->Exit();
        return;
    }

    bool result = HttpReqLogin( &authProfile );
    if ( !result ) {
        MessageError("Login failure");
        this->Exit();
        return;
    }

    mainUI = new MainUI;
    mainUI->showMaximized();
    mainUI->addNewProject( authProfile );

    QApplication::exec();
}

void MainAdaptix::Exit() {
    QApplication::exit(0);
}

void MainAdaptix::SetApplicationTheme() {

    QGuiApplication::setWindowIcon( QIcon( ":/LogoLin" ) );

    int FontID = QFontDatabase::addApplicationFont( ":/fonts/DroidSansMono" );
    QString FontFamily = QFontDatabase::applicationFontFamilies( FontID ).at( 0 );
    auto Font = QFont( FontFamily );

    Font.setPointSize( 10 );
    QApplication::setFont( Font );

    QString theme = ":/themes/Dark";
//    QString theme = ":/themes/Arc (Light)";
//    QString theme = ":/themes/Nord";

    bool result = false;
    QString style = ReadFileString(theme, &result);
    if (result) {
        QApplication *app = qobject_cast<QApplication*>(QCoreApplication::instance());
        app->setStyleSheet(style);
    }
}
