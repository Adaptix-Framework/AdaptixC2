#include "Utils/FontManager.h"
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
    // 加载应用程序字体并记录实际的字体族名
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
        {":/fonts/SpaceMono_I", "Space Mono"}
    };
    
    for (const auto& fontRes : fonts) {
        int fontId = QFontDatabase::addApplicationFont(fontRes.resourcePath);
        if (fontId != -1) {
            QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
            if (!fontFamilies.isEmpty()) {
                QString actualFamilyName = fontFamilies.first();
                m_loadedFonts[fontRes.alias] = actualFamilyName;
                // 移除字体加载成功的调试输出
            }
            // 移除字体加载失败的警告输出
        }
        // 移除字体加载失败的警告输出
    }
}

QFont FontManager::getFont(const QString& fontName, int pointSize)
{
    if (!m_initialized) {
        initialize();
    }
    
    QFont font;
    
    // 首先尝试使用加载的字体
    if (m_loadedFonts.contains(fontName)) {
        font = QFont(m_loadedFonts[fontName]);
    } else {
        // 如果没有找到，尝试直接使用字体名称
        font = QFont(fontName);
        
        // 检查字体是否真的可用
        QFontInfo fontInfo(font);
        if (fontInfo.family() != fontName) {
            // 移除字体不可用的警告输出
            font = getDefaultMonospaceFont();
        }
    }
    
    if (pointSize > 0) {
        font.setPointSize(pointSize);
    }
    
    // 确保字体是等宽的
    if (!QFontInfo(font).fixedPitch()) {
        // 移除非等宽字体的警告输出
        font = getDefaultMonospaceFont(pointSize);
    }
    
    return font;
}

bool FontManager::isFontAvailable(const QString& fontName)
{
    if (!m_initialized) {
        initialize();
    }
    
    if (m_loadedFonts.contains(fontName)) {
        return true;
    }
    
    QFont testFont(fontName);
    QFontInfo fontInfo(testFont);
    return fontInfo.family() == fontName;
}

QFont FontManager::getDefaultMonospaceFont(int pointSize)
{
    // 尝试常见的等宽字体
    QStringList monospaceFonts = {
        "Menlo",           // macOS 默认
        "Monaco",          // macOS 备选
        "Consolas",        // Windows
        "Courier New",     // 跨平台
        "monospace"        // 通用等宽字体
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
    
    // 如果都不可用，返回系统默认字体并设置为等宽
    QFont font;
    font.setFamily("monospace");
    font.setStyleHint(QFont::TypeWriter);
    if (pointSize > 0) {
        font.setPointSize(pointSize);
    }
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