#include <Utils/Convert.h>

bool IsValidURI(const QString &uri)
{
    QRegularExpression regex(R"(^\/(?!\/)([a-zA-Z0-9\-_]+\/?)*[a-zA-Z0-9\-_]$)");
    QRegularExpressionMatch match = regex.match(uri);
    return match.hasMatch();
}

QString UnixTimestampGlobalToStringLocal(qint64 timestamp)
{
    QDateTime epochDateTime = QDateTime::fromSecsSinceEpoch(timestamp, Qt::UTC);
    QDateTime localDateTime = epochDateTime.toLocalTime();
    QString formattedTime = localDateTime.toString("dd/MM hh:mm:ss");
    return formattedTime;
}