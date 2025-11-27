#ifndef TITLEBARSTYLE_H
#define TITLEBARSTYLE_H

#include <QWidget>
#include <QString>

namespace TitleBarStyle {
    void applyForTheme(QWidget* window, const QString& themeName);
}

#endif // TITLEBARSTYLE_H
