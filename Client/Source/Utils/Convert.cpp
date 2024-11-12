#include <Utils/Convert.h>

bool IsValidURI(const QString &uri)
{
    QRegularExpression regex(R"(^\/(?!\/)([a-zA-Z0-9\-_]+\/?)*[a-zA-Z0-9\-_]$)");
    QRegularExpressionMatch match = regex.match(uri);
    return match.hasMatch();
}

QString UnixTimestampGlobalToStringLocal(qint64 timestamp)
{
    if ( timestamp == 0 )
        return "";

    QDateTime epochDateTime = QDateTime::fromSecsSinceEpoch(timestamp, Qt::UTC);
    QDateTime localDateTime = epochDateTime.toLocalTime();
    QString formattedTime = localDateTime.toString("dd/MM hh:mm:ss");
    return formattedTime;
}

QString TextColorHtml(QString text, QString color)
{
    if (text.isEmpty())
        return "";

    return R"(<font color=")" + color + R"(">)" + text.toHtmlEscaped() + R"(</font>)";
}

QString TextUnderlineColorHtml(QString text, QString color)
{
    if (text.isEmpty())
        return "";

    if (color.isEmpty())
        return R"(<u>)" + text.toHtmlEscaped() + R"(</u>)";

    return R"(<font color=")" + color + R"("><u>)" + text.toHtmlEscaped() + R"(</u></font>)";
}

QString TextBoltColorHtml(QString text, QString color )
{
    if (text.isEmpty())
        return "";

    if (color.isEmpty())
        return R"(<b>)" + text.toHtmlEscaped() + R"(</b>)";

    return R"(<font color=")" + color + R"("><b>)" + text.toHtmlEscaped() + R"(</b></font>)";
}

QString FormatSecToStr(int seconds)
{
    QString result  = "";
    int     hours   = seconds / 3600;
    int     minutes = (seconds % 3600) / 60;
    int     secs    = seconds % 60;

    if (hours > 0)
        result += QString::number(hours) + "h ";
    if (minutes > 0)
        result += QString::number(minutes) + "m ";
    if (secs > 0 || result.isEmpty())
        result += QString::number(secs) + "s";

    return result.trimmed();
}

QString TrimmedEnds(QString str)
{
    return str.remove(QRegularExpression("\\s+$"));
}