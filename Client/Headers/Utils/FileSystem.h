#ifndef ADAPTIXCLIENT_FILESYSTEM_H
#define ADAPTIXCLIENT_FILESYSTEM_H

#include <QString>
#include <QIODeviceBase>
#include <QTextStream>
#include <QFile>

QString ReadFileString(const QString &filePath, bool* result);

QString GetBasenameWindows(const QString& path);

QString GetRootPathWindows(const QString& path);

QString GetParentPathWindows(const QString& path);

#endif //ADAPTIXCLIENT_FILESYSTEM_H
