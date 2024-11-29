#ifndef ADAPTIXCLIENT_FILESYSTEM_H
#define ADAPTIXCLIENT_FILESYSTEM_H

#include <QString>
#include <QIODeviceBase>
#include <QTextStream>
#include <QFile>

QString ReadFileString(const QString &filePath, bool* result);

QString GetRootPathWindows(const QString& path);

#endif //ADAPTIXCLIENT_FILESYSTEM_H
