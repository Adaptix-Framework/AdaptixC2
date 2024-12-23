#ifndef AXCHECKER_UTILS_H
#define AXCHECKER_UTILS_H

#include <main.h>

QString ReadFileString(const QString &filePath, bool* result);

QByteArray ReadFileBytearray(const QString &filePath, bool* result);

QString ValidCommandsFile(QByteArray jsonData, bool* result);

QString ValidCommand(QJsonObject extJsonObject, bool* result);

QString ValidExtCommand(QJsonObject extJsonObject, bool* result);

#endif //AXCHECKER_UTILS_H
