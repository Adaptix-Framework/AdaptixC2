#ifndef QTFONTFIX_H
#define QTFONTFIX_H

#include <QLoggingCategory>

class QtFontFix
{
public:
    // 初始化Qt字体修复，抑制OpenType警告
    static void initialize();
    
    // 设置Qt日志过滤规则
    static void setupLoggingFilters();
    
private:
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    static bool isInitialized;
};

#endif // QTFONTFIX_H