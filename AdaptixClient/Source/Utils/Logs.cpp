#include <Utils/Logs.h>

void LogInfo(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    QString message = QString::vasprintf(format, args);
    va_end(args);

    qInfo().nospace() << message;
}

void LogSuccess(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    QString message = QString::vasprintf(format, args);
    va_end(args);

    qDebug().nospace() << "\033[1;32m[+]\033[0m " << message;
}

void LogError(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    QString message = QString::vasprintf(format, args);
    va_end(args);

    qCritical().nospace() << "\033[1;31m[-]\033[0m " << message;
}

void MessageError(const QString &message )
{
    auto box = QMessageBox();
    box.setWindowTitle( "Error" );
    box.setText( message );
    box.setIcon( QMessageBox::Critical );
    box.exec();
}

void MessageSuccess(const QString &message )
{
    auto box = QMessageBox();
    box.setWindowTitle( "Success" );
    box.setText( message );
    box.setIcon( QMessageBox::Information );
    box.exec();
}