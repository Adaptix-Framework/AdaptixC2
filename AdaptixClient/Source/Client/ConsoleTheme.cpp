#include <Client/ConsoleTheme.h>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QApplication>

#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/style/Theme.hpp>

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
    QString css = QString("background-color: %1; border: 1px solid #2A2A2A; border-radius: 4px;").arg(color.name());
    if (showImage && type == Image && !imagePath.isEmpty()) {
        css += QString(" background-image: url(%1); background-position: center; background-repeat: no-repeat; opacity: %2;").arg(imagePath).arg(dimming / 100.0);
    }
    return css;
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
        if (st.contains("debug"))
            theme.statusDebug = QColor(st["debug"].toString());
        if (st.contains("warn"))
            theme.statusWarn = QColor(st["warn"].toString());
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

bool ConsoleThemeManager::deleteTheme(const QString& name)
{
    QString userPath = userThemeDir() + "/" + name + ".json";
    if (!QFile::exists(userPath))
        return false;
    return QFile::remove(userPath);
}

ConsoleThemeData ConsoleThemeManager::buildFromQlementine(const QString& themeName, const QString& bgImagePath, int bgDimming)
{
    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp->style());
    const auto& t = qs ? qs->theme() : oclero::qlementine::Theme();

    QJsonObject consoleKeys;
    QString name = themeName;
    if (name.isEmpty())
        name = t.meta.name;

    QString resPath  = QString(":/qlementine-themes/%1.json").arg(name);
    QString userPath = QDir(QDir::homePath()).filePath(".adaptix/themes/app/" + name + ".json");
    QString jsonPath = QFile::exists(userPath) ? userPath : (QFile::exists(resPath) ? resPath : QString());

    if (!jsonPath.isEmpty()) {
        QFile file(jsonPath);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            file.close();
            QJsonObject root = doc.object();
            if (root.contains("console"))
                consoleKeys = root["console"].toObject();
        }
    }

    qreal lum = 0.2126 * t.backgroundColorMain1.redF()
              + 0.7152 * t.backgroundColorMain1.greenF()
              + 0.0722 * t.backgroundColorMain1.blueF();
    bool isDark = lum < 0.5;

    QColor infoColor    = isDark ? QColor("#4FC3F7") : QColor("#1976D2");  // blue
    QColor successColor = isDark ? QColor("#FFD740") : QColor("#F9A825");  // yellow
    QColor errorColor   = isDark ? QColor("#FF5252") : QColor("#D32F2F");  // red
    QColor debugColor   = isDark ? QColor("#78909C") : QColor("#78909C");  // blue-gray
    QColor warnColor    = isDark ? QColor("#FFB740") : QColor("#E65100");  // orange

    auto readColor = [](const QJsonObject& obj, const QString& key, const QColor& fallback) -> QColor {
        return obj.contains(key) ? QColor(obj[key].toString()) : fallback;
    };
    infoColor    = readColor(consoleKeys, "statusInfo",    infoColor);
    successColor = readColor(consoleKeys, "statusSuccess", successColor);
    errorColor   = readColor(consoleKeys, "statusError",   errorColor);
    debugColor   = readColor(consoleKeys, "statusDebug",   debugColor);
    warnColor    = readColor(consoleKeys, "statusWarn",    warnColor);

    ConsoleThemeData theme;

    QString effectiveBgPath = bgImagePath;
    if (effectiveBgPath.isEmpty())
        effectiveBgPath = ":/Back";

    if (QFile::exists(effectiveBgPath)) {
        theme.background.type     = ConsoleBackground::Image;
        theme.background.imagePath = effectiveBgPath;
        theme.background.dimming  = bgDimming;
        theme.background.color    = t.backgroundColorMain1;
    } else {
        theme.background.color = t.backgroundColorMain1;
    }

    theme.textColor     = t.secondaryColor;
    theme.statusInfo    = infoColor;
    theme.statusSuccess = successColor;
    theme.statusError   = errorColor;
    theme.statusDebug   = debugColor;
    theme.statusWarn    = warnColor;

    theme.debug         = ConsoleStyleEntry(t.secondaryAlternativeColor);
    theme.operatorStyle = ConsoleStyleEntry(t.secondaryColor);
    theme.task          = ConsoleStyleEntry(t.secondaryAlternativeColor);
    theme.agent         = ConsoleStyleEntry(t.primaryColor, false, false, true);
    theme.command       = ConsoleStyleEntry(t.secondaryColor, true);
    theme.input.symbol  = QStringLiteral(">");
    theme.input.style   = ConsoleStyleEntry(t.primaryColor);

    theme.logDebug            = ConsoleStyleEntry(t.secondaryAlternativeColor);
    theme.operatorConnect     = ConsoleStyleEntry(successColor);
    theme.operatorDisconnect  = ConsoleStyleEntry(errorColor);
    theme.agentNew            = ConsoleStyleEntry(successColor);
    theme.tunnel              = ConsoleStyleEntry(warnColor);
    theme.listenerStart       = ConsoleStyleEntry(successColor);
    theme.listenerStop        = ConsoleStyleEntry(errorColor);

    return theme;
}
