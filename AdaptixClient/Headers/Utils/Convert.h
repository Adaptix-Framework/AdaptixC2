#ifndef ADAPTIXCLIENT_CONVERT_H
#define ADAPTIXCLIENT_CONVERT_H

#include <main.h>

bool IsValidURI(const QString &uri);

QString ValidCommandsFile(const QByteArray &jsonData, bool* result);

QString ValidCommand(QJsonObject extJsonObject, bool* result);

QString ValidExtCommand(QJsonObject extJsonObject, bool* result);

QString ValidExtConstant(QJsonObject extJsonObject, bool* result);

QString UnixTimestampGlobalToStringLocal(qint64 timestamp);

QString UnixTimestampGlobalToStringLocalSmall(qint64 timestamp);

QString UnixTimestampGlobalToStringLocalFull(qint64 timestamp);

QString TextColorHtml(const QString &text, const QString &color);

// QString TextUnderlineColorHtml(const QString &text, const QString &color = "");
//
// QString TextBoltColorHtml(const QString &text, const QString &color = "");

QString FormatSecToStr(int seconds);

QString TrimmedEnds(QString str);

QString BytesToFormat(qint64 bytes);

QIcon RecolorIcon(QIcon originalIcon, const QString &colorString);

QString GenerateRandomString(const int length, const QString &setName);

QString GenerateHash(const QString &algorithm, int length, const QString &inputString);

#endif