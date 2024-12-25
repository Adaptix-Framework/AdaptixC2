#ifndef ADAPTIXCLIENT_CONVERT_H
#define ADAPTIXCLIENT_CONVERT_H

#include <QString>
#include <QRegularExpression>
#include <QDateTime>
#include <QPixmap>
#include <QIcon>
#include <QPainter>

#include <main.h>

bool IsValidURI(const QString &uri);

QString ValidCommandsFile(QByteArray jsonData, bool* result);

QString ValidCommand(QJsonObject extJsonObject, bool* result);

QString ValidExtCommand(QJsonObject extJsonObject, bool* result);

QString UnixTimestampGlobalToStringLocal(qint64 timestamp);

QString UnixTimestampGlobalToStringLocalFull(qint64 timestamp);

QString TextColorHtml(QString text, QString color);

QString TextUnderlineColorHtml(QString text, QString color = "");

QString TextBoltColorHtml(QString text, QString color = "");

QString FormatSecToStr(int seconds);

QString TrimmedEnds(QString str);

QString BytesToFormat(qint64 bytes);

QIcon RecolorIcon(QIcon originalIcon, QString colorString);

#endif //ADAPTIXCLIENT_CONVERT_H