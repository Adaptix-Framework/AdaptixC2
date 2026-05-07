#ifndef ADAPTIXCLIENT_CONSOLETHEME_H
#define ADAPTIXCLIENT_CONSOLETHEME_H

#include <main.h>
#include <QTextCharFormat>

struct ConsoleStyleEntry
{
    QColor color;
    bool   bold      = false;
    bool   italic    = false;
    bool   underline = false;

    ConsoleStyleEntry() = default;
    ConsoleStyleEntry(const QColor& c, bool b = false, bool i = false, bool u = false) : color(c), bold(b), italic(i), underline(u) {}

    QTextCharFormat toFormat() const;
    static ConsoleStyleEntry fromJson(const QJsonObject& obj);
    static ConsoleStyleEntry fromJson(const QJsonObject& obj, const ConsoleStyleEntry& fallback);
};

struct ConsoleBackground
{
    enum Type { Color, Image };

    Type    type       = Color;
    QColor  color      = QColor("#151515");
    QString imagePath;
    int     dimming    = 80;

    QString toStyleSheet(bool showImage = true) const;
    QString toColorOnlyStyleSheet() const;
    static ConsoleBackground fromJson(const QJsonObject& obj);
};

struct ConsoleThemeData
{
    ConsoleBackground background;
    QColor textColor = QColor("#E0E0E0");

    // Agent console
    ConsoleStyleEntry debug;
    QColor statusSuccess = QColor("#FFFF00");
    QColor statusError   = QColor("#E32227");
    QColor statusInfo    = QColor("#89CFF0");

    ConsoleStyleEntry operatorStyle;
    ConsoleStyleEntry task;
    ConsoleStyleEntry agent;
    ConsoleStyleEntry command;

    struct InputPrompt {
        QString symbol = ">";
        ConsoleStyleEntry style;
    } input;

    // Log console
    ConsoleStyleEntry logDebug;
    ConsoleStyleEntry operatorConnect;
    ConsoleStyleEntry operatorDisconnect;
    ConsoleStyleEntry agentNew;
    ConsoleStyleEntry tunnel;
    ConsoleStyleEntry listenerStart;
    ConsoleStyleEntry listenerStop;

    static ConsoleThemeData fromJson(const QJsonObject& root);
};

class ConsoleThemeManager : public QObject
{
Q_OBJECT
    ConsoleThemeManager() = default;
    QString resolveThemePath(const QString& name) const;

    ConsoleThemeData m_theme;
    QString m_themeName;

public:
    static ConsoleThemeManager& instance();

    QStringList availableThemes() const;
    void loadTheme(const QString& name);
    bool importTheme(const QString& filePath);

    const ConsoleThemeData& theme() const { return m_theme; }
    QString currentThemeName() const { return m_themeName; }

    static QString userThemeDir();

Q_SIGNALS:
    void themeChanged();
};

#endif
