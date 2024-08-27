#include <QApplication>
#include <QPushButton>
#include <QFontDatabase>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    auto FontID = QFontDatabase::addApplicationFont( ":/fonts/DroidSansMono" );
    auto Family = QFontDatabase::applicationFontFamilies( FontID ).at( 0 );
    auto Font = QFont( Family );
    Font.setPointSize( 10 );
    QApplication::setFont( Font );
    QGuiApplication::setWindowIcon( QIcon( ":/LogoLin" ) );

    QPushButton button("Hello world!", nullptr);
    button.resize(200, 100);
    button.show();

    return QApplication::exec();
}
