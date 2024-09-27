#ifndef AXCHECKER_UTILS_H
#define AXCHECKER_UTILS_H

#include <main.h>

QString ReadFileString(const QString &filePath, bool* result);

QByteArray ReadFileBytearray(const QString &filePath, bool* result);

#endif //AXCHECKER_UTILS_H
