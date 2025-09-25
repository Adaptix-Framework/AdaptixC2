#ifndef ADAPTIXCLIENT_FONTMANAGER_H
#define ADAPTIXCLIENT_FONTMANAGER_H

#include <QFont>
#include <QFontDatabase>
#include <QString>
#include <QMap>

class FontManager
{
    QMap<QString, QString> m_loadedFonts;
    bool m_initialized = false;

    FontManager() = default;
    ~FontManager() = default;
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;

    void loadApplicationFonts();
    QString findBestMonospaceFont();

public:
    static FontManager& instance();

    void  initialize();
    QFont getFont(const QString& fontName, int pointSize = -1);
    bool  isFontAvailable(const QString& fontName);
    QFont getDefaultMonospaceFont(int pointSize = -1);
};

#endif