#include <Utils/Convert.h>

bool IsValidURI(const QString &uri)
{
    QRegularExpression regex(R"(^\/(?!\/)([a-zA-Z0-9\-_]+\/?)*[a-zA-Z0-9\-_]$)");
    QRegularExpressionMatch match = regex.match(uri);
    return match.hasMatch();
}


QString ValidCommandsFile(const QByteArray &jsonData, bool* result)
{
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError && document.isObject()) {
        *result = false;
        return QString("JSON parse error: %1").arg(parseError.errorString());
    }

    if(!document.isArray()) {
        *result = false;
        return "Error Commander Json Format";
    }

    QJsonArray commandArray = document.array();
    for (QJsonValue value : commandArray) {
        QJsonObject jsonObject = value.toObject();
        QString ret = ValidCommand(jsonObject, result);
        if( ! *result )
            return ret;
    }

    return "";
}

QString ValidCommand(QJsonObject extJsonObject, bool* result)
{
    QRegularExpression regex(R"((\w+)\s+([\[\<][^\s\]]+[\s\w-]*[\>\]])(\s*\([^\)]*\))?(?:\s+\{(.+)\})?)");

    if( !extJsonObject.contains("command") || !extJsonObject["command"].isString() ) {
        *result = false;
        return "Extension must include a required 'command' parameter";
    }
    QString command = extJsonObject["command"].toString();

    if (extJsonObject.contains("subcommands")) {
        QJsonArray subcommandArray = extJsonObject["subcommands"].toArray();
        for (QJsonValue subCmdVal : subcommandArray) {
            QJsonObject subCmdObj = subCmdVal.toObject();

            if( !subCmdObj.contains("name") || !subCmdObj["name"].isString() ) {
                *result = false;
                return QString("The command '%1' does not contain a 'name' parameter in Subcommand block").arg(command);
            }
            QString subcommand = subCmdObj["name"].toString();

            QJsonArray subArgsArray = subCmdObj["args"].toArray();
            for (QJsonValue subArgVal : subArgsArray) {
                QRegularExpressionMatch match = regex.match( subArgVal.toString() );
                if ( !match.hasMatch()) {
                    *result = false;
                    return "Arguments not parsed for command: " + command + " " + subcommand;
                }

                QString flagAndValue = match.captured(2).trimmed();
                if (!((flagAndValue.startsWith('<') && flagAndValue.endsWith('>')) || (flagAndValue.startsWith('[') && flagAndValue.endsWith(']')))) {
                    *result = false;
                    return "Argument must be in <> or [] for command: " + command + " " + subcommand;
                }
            }
        }
    } else {
        if (extJsonObject.contains("args")) {
            QJsonArray argsArray = extJsonObject["args"].toArray();
            for (QJsonValue argVal : argsArray) {
                QRegularExpressionMatch match = regex.match( argVal.toString() );
                if ( !match.hasMatch()) {
                    *result = false;
                    return "Arguments not parsed for command: " + command;
                }

                QString flagAndValue = match.captured(2).trimmed();
                if (!((flagAndValue.startsWith('<') && flagAndValue.endsWith('>')) || (flagAndValue.startsWith('[') && flagAndValue.endsWith(']')))) {
                    *result = false;
                    return "Argument must be in <> or [] for command: " + command;
                }
            }
        }
    }
    return "";
}

QString ValidExtCommand(QJsonObject extJsonObject, bool* result)
{
    QRegularExpression regex(R"((\w+)\s+([\[\<][^\s\]]+[\s\w-]*[\>\]])(\s*\([^\)]*\))?(?:\s+\{(.+)\})?)");

    if( !extJsonObject.contains("command") || !extJsonObject["command"].isString() ) {
        *result = false;
        return "Extension must include a required 'command' parameter";
    }
    QString command = extJsonObject["command"].toString();

    if (extJsonObject.contains("subcommands")) {
        QJsonArray subcommandArray = extJsonObject["subcommands"].toArray();
        for (QJsonValue subCmdVal : subcommandArray) {
            QJsonObject subCmdObj = subCmdVal.toObject();

            if( !subCmdObj.contains("name") || !subCmdObj["name"].isString() ) {
                *result = false;
                return QString("The command '%1' does not contain a 'name' parameter in Subcommand block").arg(command);
            }

            QString subcommand = subCmdObj["name"].toString();

            if( !subCmdObj.contains("exec") || !subCmdObj["exec"].isString() ) {
                *result = false;
                return "Extension must include a required 'exec' parameter for command: " + command + " " + subcommand;
            }

            QJsonArray subArgsArray = subCmdObj["args"].toArray();
            for (QJsonValue subArgVal : subArgsArray) {
                QRegularExpressionMatch match = regex.match( subArgVal.toString() );
                if ( !match.hasMatch()) {
                    *result = false;
                    return "Arguments not parsed for command: " + command + " " + subcommand;
                }

                QString flagAndValue = match.captured(2).trimmed();
                if (!((flagAndValue.startsWith('<') && flagAndValue.endsWith('>')) || (flagAndValue.startsWith('[') && flagAndValue.endsWith(']')))) {
                    *result = false;
                    return "Argument must be in <> or [] for command: " + command + " " + subcommand;
                }
            }
        }
    } else {
        if( !extJsonObject.contains("exec") || !extJsonObject["exec"].isString() ) {
            *result = false;
            return "Extension must include a required 'exec' parameter for command: " + command;
        }
        if (extJsonObject.contains("args")) {
            QJsonArray argsArray = extJsonObject["args"].toArray();
            for (QJsonValue argVal : argsArray) {
                QRegularExpressionMatch match = regex.match( argVal.toString() );
                if ( !match.hasMatch()) {
                    *result = false;
                    return "Arguments not parsed for command: " + command;
                }

                QString flagAndValue = match.captured(2).trimmed();
                if (!((flagAndValue.startsWith('<') && flagAndValue.endsWith('>')) || (flagAndValue.startsWith('[') && flagAndValue.endsWith(']')))) {
                    *result = false;
                    return "Argument must be in <> or [] for command: " + command;
                }
            }
        }
    }
    return "";
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

QString UnixTimestampGlobalToStringLocalSmall(qint64 timestamp)
{
    if ( timestamp == 0 )
        return "";

    QDateTime epochDateTime = QDateTime::fromSecsSinceEpoch(timestamp, Qt::UTC);
    QDateTime localDateTime = epochDateTime.toLocalTime();
    QString formattedTime = localDateTime.toString("dd/MM hh:mm");
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

QString TextColorHtml(const QString &text, const QString &color)
{
    if (text.isEmpty())
        return "";

    return R"(<font color=")" + color + R"(">)" + text.toHtmlEscaped() + R"(</font>)";
}

QString TextUnderlineColorHtml(const QString &text, const QString &color)
{
    if (text.isEmpty())
        return "";

    if (color.isEmpty())
        return R"(<u>)" + text.toHtmlEscaped() + R"(</u>)";

    return R"(<font color=")" + color + R"("><u>)" + text.toHtmlEscaped() + R"(</u></font>)";
}

QString TextBoltColorHtml(const QString &text, const QString &color )
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
    constexpr double KB = 1024.0;
    constexpr double MB = KB * 1024;
    constexpr double GB = MB * 1024;

    if (bytes >= GB) {
        return QString::number(bytes / GB, 'f', 2) + " Gb";
    } else if (bytes >= MB) {
        return QString::number(bytes / MB, 'f', 2) + " Mb";
    } else {
        return QString::number(bytes / KB, 'f', 2) + " Kb";
    }
}

QIcon RecolorIcon(QIcon originalIcon, const QString &colorString)
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