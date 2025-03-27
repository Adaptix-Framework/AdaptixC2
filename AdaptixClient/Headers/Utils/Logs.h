#ifndef ADAPTIXCLIENT_LOGS_H
#define ADAPTIXCLIENT_LOGS_H

#include <QString>
#include <QDebug>
#include <QMessageBox>
#include <cstdarg>

void LogInfo(const char* format, ...);

void LogSuccess(const char* format, ...);

void LogError(const char* format, ...);

void MessageError(const QString &message );

void MessageSuccess(const QString &message );

#endif //ADAPTIXCLIENT_LOGS_H
