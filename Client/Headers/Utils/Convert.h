#ifndef ADAPTIXCLIENT_CONVERT_H
#define ADAPTIXCLIENT_CONVERT_H

#include <QString>
#include <QRegularExpression>
#include <QDateTime>

bool IsValidURI(const QString &uri);

QString UnixTimestampGlobalToStringLocal(qint64 timestamp);

QString TextColorHtml(QString text, QString color);

QString TextUnderlineColorHtml(QString text, QString color = "");

QString TextBoltColorHtml(QString text, QString color = "");

QString FormatSecToStr(int seconds);

QString TrimmedEnds(QString str);

QString BytesToFormat(int bytes);

#endif //ADAPTIXCLIENT_CONVERT_H