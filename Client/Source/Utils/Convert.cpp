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

QString UnixTimestampGlobalToStringLocalFull(qint64 timestamp)
{
    if ( timestamp == 0 )
        return "";

    QDateTime epochDateTime = QDateTime::fromSecsSinceEpoch(timestamp, Qt::UTC);
    QDateTime localDateTime = epochDateTime.toLocalTime();
    QString formattedTime = localDateTime.toString("hh:mm dd/MM/yyyy");
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

QString BytesToFormat(qint64 bytes)
{
    const double KB = 1024.0;
    const double MB = KB * 1024;
    const double GB = MB * 1024;

    if (bytes >= GB) {
        return QString::number(bytes / GB, 'f', 2) + " Gb";
    } else if (bytes >= MB) {
        return QString::number(bytes / MB, 'f', 2) + " Mb";
    } else {
        return QString::number(bytes / KB, 'f', 2) + " Kb";
    }
}

QIcon RecolorIcon(QIcon originalIcon, QString colorString)
{
    QColor color = QColor(colorString);
    if ( !color.isValid() )
        return originalIcon;

    QPixmap pixmap = originalIcon.pixmap(originalIcon.actualSize(QSize(128, 128)));

    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn); // Recoloring only pixels with content
    painter.fillRect(pixmap.rect(), color);
    painter.end();

    return QIcon(pixmap);
}