#ifndef FONTMANAGER_H
#define FONTMANAGER_H

#include <QFont>
#include <QFontDatabase>
#include <QString>
#include <QMap>

class FontManager
{
public:
    static FontManager& instance();
    
    // 初始化字体管理器，加载所有应用字体
    void initialize();
    
    // 获取指定字体，如果不存在则返回系统默认等宽字体
    QFont getFont(const QString& fontName, int pointSize = -1);
    
    // 检查字体是否可用
    bool isFontAvailable(const QString& fontName);
    
    // 获取默认等宽字体
    QFont getDefaultMonospaceFont(int pointSize = -1);

private:
    FontManager() = default;
    ~FontManager() = default;
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;
    
    QMap<QString, QString> m_loadedFonts; // 映射：别名 -> 实际字体族名
    bool m_initialized = false;
    
    void loadApplicationFonts();
    QString findBestMonospaceFont();
};

#endif // FONTMANAGER_H