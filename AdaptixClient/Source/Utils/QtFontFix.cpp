#include "Utils/QtFontFix.h"
#include <QLoggingCategory>
#include <QDebug>
#include <QString>
#include <QCoreApplication>

bool QtFontFix::isInitialized = false;

void QtFontFix::initialize()
{
    if (isInitialized) {
        return;
    }
    
    // 只使用日志过滤规则，不安装自定义消息处理器
    setupLoggingFilters();
    
    isInitialized = true;
    // 移除初始化完成的调试输出
}

void QtFontFix::setupLoggingFilters()
{
    // 禁用Qt字体数据库的所有输出
    QLoggingCategory::setFilterRules(
        "qt.text.font.db=false\n"
        "qt.text.font.db.debug=false\n"
        "qt.text.font.db.warning=false\n"
        "qt.text.font.db.info=false\n"
        "qt.text.font.db.critical=false"
    );
}

void QtFontFix::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // 这个函数现在不被使用，保留以备将来需要
    Q_UNUSED(type)
    Q_UNUSED(context)
    Q_UNUSED(msg)
}