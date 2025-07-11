#ifndef ADAPTIXCLIENT_FILESYSTEM_H
#define ADAPTIXCLIENT_FILESYSTEM_H

#include <QString>
#include <QIODeviceBase>
#include <QTextStream>
#include <QFile>
#include <QIcon>

#define TYPE_FILE    0
#define TYPE_DIR     1
#define TYPE_DISK    2
#define TYPE_ROOTDIR 3

QString ReadFileString(const QString &filePath, bool* result);

QString GetBasenameWindows(const QString& path);

QString GetBasenameUnix(const QString& path);

QString GetRootPathWindows(const QString& path);

QString GetRootPathUnix(const QString& path);

QString GetParentPathWindows(const QString& path);

QString GetParentPathUnix(const QString& path);

QIcon GetFileSystemIcon(int type, bool used);

#endif
