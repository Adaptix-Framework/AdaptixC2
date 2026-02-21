#include <Utils/FontManager.h>
#include <QDebug>
#include <QFontInfo>

FontManager& FontManager::instance()
{
    static FontManager instance;
    return instance;
}

void FontManager::initialize()
{
    if (m_initialized) {
        return;
    }

    loadApplicationFonts();
    m_initialized = true;
}

void FontManager::loadApplicationFonts()
{
    struct FontResource {
        QString resourcePath;
        QString alias;
    };

    QList<FontResource> fonts = {
        {":/fonts/DroidSansMono", "Droid Sans Mono"},
        {":/fonts/VT323", "VT323"},
        {":/fonts/Anonymous", "Anonymous Pro"},
        {":/fonts/Anonymous_B", "Anonymous Pro"},
        {":/fonts/Anonymous_BI", "Anonymous Pro"},
        {":/fonts/Anonymous_I", "Anonymous Pro"},
        {":/fonts/DejavuSansMono", "DejaVu Sans Mono"},
        {":/fonts/DejavuSansMono_B", "DejaVu Sans Mono"},
        {":/fonts/DejavuSansMono_BO", "DejaVu Sans Mono"},
        {":/fonts/DejavuSansMono_O", "DejaVu Sans Mono"},
        {":/fonts/Hack", "Hack"},
        {":/fonts/Hack_B", "Hack"},
        {":/fonts/Hack_BI", "Hack"},
        {":/fonts/Hack_I", "Hack"},
        {":/fonts/SpaceMono", "Space Mono"},
        {":/fonts/SpaceMono_B", "Space Mono"},
        {":/fonts/SpaceMono_BI", "Space Mono"},
        {":/fonts/SpaceMono_I", "Space Mono"},
        {":/fonts/JetBrainsMono", "JetBrains Mono"},
        {":/fonts/JetBrainsMono_B", "JetBrains Mono"},
        {":/fonts/JetBrainsMono_BI", "JetBrains Mono"},
        {":/fonts/JetBrainsMono_I", "JetBrains Mono"}
    };

    for (const auto& fontRes : fonts) {
        int fontId = QFontDatabase::addApplicationFont(fontRes.resourcePath);
        if (fontId != -1) {
            QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
            if (!fontFamilies.isEmpty()) {
                QString actualFamilyName = fontFamilies.first();
                m_loadedFonts[fontRes.alias] = actualFamilyName;
            }
        }
    }
}

QFont FontManager::getFont(const QString& fontName, int pointSize)
{
    if (!m_initialized)
        initialize();

    QFont font;

    if (m_loadedFonts.contains(fontName)) {
        font = QFont(m_loadedFonts[fontName]);
    } else {
        font = QFont(fontName);

        QFontInfo fontInfo(font);
        if (fontInfo.family() != fontName)
            font = getDefaultMonospaceFont();
    }

    if (pointSize > 0)
        font.setPointSize(pointSize);

    if (!QFontInfo(font).fixedPitch())
        font = getDefaultMonospaceFont(pointSize);

    return font;
}

bool FontManager::isFontAvailable(const QString& fontName)
{
    if (!m_initialized)
        initialize();

    if (m_loadedFonts.contains(fontName))
        return true;

    QFont testFont(fontName);
    QFontInfo fontInfo(testFont);
    return fontInfo.family() == fontName;
}

QFont FontManager::getDefaultMonospaceFont(int pointSize)
{
    QStringList monospaceFonts = {
        "Menlo",           // macOS Default
        "Monaco",          // macOS Alternative
        "Consolas",        // Windows
        "Courier New",     // Cross-platform
        "monospace"        // Universal
    };

    for (const QString& fontName : monospaceFonts) {
        QFont font(fontName);
        QFontInfo fontInfo(font);
        if (fontInfo.fixedPitch()) {
            if (pointSize > 0) {
                font.setPointSize(pointSize);
            }
            return font;
        }
    }

    QFont font;
    font.setFamily("monospace");
    font.setStyleHint(QFont::TypeWriter);
    if (pointSize > 0)
        font.setPointSize(pointSize);

    return font;
}

QString FontManager::findBestMonospaceFont()
{
    QStringList candidates = {"Menlo", "Monaco", "Consolas", "Courier New"};

    for (const QString& candidate : candidates) {
        QFont font(candidate);
        QFontInfo fontInfo(font);
        if (fontInfo.fixedPitch()) {
            return candidate;
        }
    }

    return "monospace";
}