#include <Client/ConsoleTheme.h>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>

QTextCharFormat ConsoleStyleEntry::toFormat() const
{
    QTextCharFormat fmt;
    if (color.isValid())
        fmt.setForeground(color);
    if (bold)
        fmt.setFontWeight(QFont::Bold);
    if (italic)
        fmt.setFontItalic(true);
    if (underline)
        fmt.setFontUnderline(true);
    return fmt;
}

ConsoleStyleEntry ConsoleStyleEntry::fromJson(const QJsonObject& obj)
{
    return fromJson(obj, ConsoleStyleEntry());
}

ConsoleStyleEntry ConsoleStyleEntry::fromJson(const QJsonObject& obj, const ConsoleStyleEntry& fallback)
{
    ConsoleStyleEntry entry = fallback;
    if (obj.contains("color"))
        entry.color = QColor(obj["color"].toString());
    if (obj.contains("bold"))
        entry.bold = obj["bold"].toBool();
    if (obj.contains("italic"))
        entry.italic = obj["italic"].toBool();
    if (obj.contains("underline"))
        entry.underline = obj["underline"].toBool();
    return entry;
}

QString ConsoleBackground::toStyleSheet(bool showImage) const
{
    return QString("background-color: %1; border: 1px solid #2A2A2A; border-radius: 4px;").arg(color.name());
}

QString ConsoleBackground::toColorOnlyStyleSheet() const
{
    return QString("background-color: %1; border: 1px solid #2A2A2A; border-radius: 4px;").arg(color.name());
}

ConsoleBackground ConsoleBackground::fromJson(const QJsonObject& obj)
{
    ConsoleBackground bg;
    if (obj.contains("color"))
        bg.color = QColor(obj["color"].toString());
    if (obj.contains("image")) {
        bg.type = Image;
        bg.imagePath = obj["image"].toString();
        if (obj.contains("dimming"))
            bg.dimming = qBound(0, obj["dimming"].toInt(70), 100);
    }
    return bg;
}

ConsoleThemeData ConsoleThemeData::fromJson(const QJsonObject& root)
{
    ConsoleThemeData theme;

    if (root.contains("background"))
        theme.background = ConsoleBackground::fromJson(root["background"].toObject());

    if (root.contains("text"))
        theme.textColor = QColor(root["text"].toString());

    // Agent console section
    QJsonObject console = root.contains("console") ? root["console"].toObject() : QJsonObject();

    if (console.contains("debug"))
        theme.debug = ConsoleStyleEntry::fromJson(console["debug"].toObject(), ConsoleStyleEntry(QColor("#606060")));

    if (console.contains("status")) {
        QJsonObject st = console["status"].toObject();
        if (st.contains("success"))
            theme.statusSuccess = QColor(st["success"].toString());
        if (st.contains("error"))
            theme.statusError = QColor(st["error"].toString());
        if (st.contains("info"))
            theme.statusInfo = QColor(st["info"].toString());
    }

    if (console.contains("operator"))
        theme.operatorStyle = ConsoleStyleEntry::fromJson(console["operator"].toObject(), ConsoleStyleEntry(QColor("#808080")));

    if (console.contains("task"))
        theme.task = ConsoleStyleEntry::fromJson(console["task"].toObject(), ConsoleStyleEntry(QColor("#606060")));

    if (console.contains("agent"))
        theme.agent = ConsoleStyleEntry::fromJson(console["agent"].toObject(), ConsoleStyleEntry(QColor("#808080"), false, false, true));

    if (console.contains("command"))
        theme.command = ConsoleStyleEntry::fromJson(console["command"].toObject(), ConsoleStyleEntry(QColor("#E0E0E0"), true));

    if (console.contains("input")) {
        QJsonObject inp = console["input"].toObject();
        if (inp.contains("symbol"))
            theme.input.symbol = inp["symbol"].toString();
        theme.input.style = ConsoleStyleEntry::fromJson(inp, ConsoleStyleEntry(QColor("#808080")));
    }

    // Log section
    QJsonObject log = root.contains("log") ? root["log"].toObject() : QJsonObject();

    if (log.contains("debug"))
        theme.logDebug = ConsoleStyleEntry::fromJson(log["debug"].toObject(), ConsoleStyleEntry(QColor("#606060")));

    if (log.contains("operator_connect"))
        theme.operatorConnect = ConsoleStyleEntry::fromJson(log["operator_connect"].toObject(), ConsoleStyleEntry(QColor("#E0E0E0")));

    if (log.contains("operator_disconnect"))
        theme.operatorDisconnect = ConsoleStyleEntry::fromJson(log["operator_disconnect"].toObject(), ConsoleStyleEntry(QColor("#808080")));

    if (log.contains("agent_new"))
        theme.agentNew = ConsoleStyleEntry::fromJson(log["agent_new"].toObject(), ConsoleStyleEntry(QColor("#39FF14")));

    if (log.contains("tunnel"))
        theme.tunnel = ConsoleStyleEntry::fromJson(log["tunnel"].toObject(), ConsoleStyleEntry(QColor("#FDFD96")));

    if (log.contains("listener_start"))
        theme.listenerStart = ConsoleStyleEntry::fromJson(log["listener_start"].toObject(), ConsoleStyleEntry(QColor("#FFA500")));

    if (log.contains("listener_stop"))
        theme.listenerStop = ConsoleStyleEntry::fromJson(log["listener_stop"].toObject(), ConsoleStyleEntry(QColor("#FFA500")));

    return theme;
}

ConsoleThemeManager& ConsoleThemeManager::instance()
{
    static ConsoleThemeManager mgr;
    return mgr;
}

QString ConsoleThemeManager::userThemeDir()
{
    QString dir = QDir(QDir::homePath()).filePath(".adaptix/themes/console");
    QDir().mkpath(dir);
    return dir;
}

QStringList ConsoleThemeManager::availableThemes() const
{
    QStringList themes;

    QDir resDir(":/console-themes");
    for (const auto& entry : resDir.entryList({"*.json"}, QDir::Files))
        themes << QFileInfo(entry).baseName();

    QDir userDir(userThemeDir());
    for (const auto& entry : userDir.entryList({"*.json"}, QDir::Files)) {
        QString name = QFileInfo(entry).baseName();
        if (!themes.contains(name))
            themes << name;
    }

    return themes;
}

QString ConsoleThemeManager::resolveThemePath(const QString& name) const
{
    QString userPath = userThemeDir() + "/" + name + ".json";
    if (QFile::exists(userPath))
        return userPath;
    return QString(":/console-themes/%1.json").arg(name);
}

void ConsoleThemeManager::loadTheme(const QString& name)
{
    m_themeName = name;

    QString path = resolveThemePath(name);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_theme = ConsoleThemeData();
        Q_EMIT themeChanged();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    m_theme = ConsoleThemeData::fromJson(doc.object());
    Q_EMIT themeChanged();
}

bool ConsoleThemeManager::importTheme(const QString& filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists() || fi.suffix().toLower() != "json")
        return false;

    QString destDir = userThemeDir();
    QString destPath = destDir + "/" + fi.fileName();
    if (QFile::exists(destPath))
        QFile::remove(destPath);

    return QFile::copy(filePath, destPath);
}
