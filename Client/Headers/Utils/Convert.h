#ifndef ADAPTIXCLIENT_CONVERT_H
#define ADAPTIXCLIENT_CONVERT_H

#include <QString>
#include <QRegularExpression>
#include <QDateTime>

bool IsValidURI(const QString &uri);

QString UnixTimestampGlobalToStringLocal(qint64 timestamp);

#endif //ADAPTIXCLIENT_CONVERT_H