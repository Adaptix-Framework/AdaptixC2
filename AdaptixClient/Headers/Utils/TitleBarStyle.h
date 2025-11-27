#ifndef ADAPTIXCLIENT_TITLEBARSTYLE_H
#define ADAPTIXCLIENT_TITLEBARSTYLE_H

#include <QWidget>
#include <QString>

namespace TitleBarStyle {
    void applyForTheme(QWidget* window, const QString& themeName);
}

#endif //ADAPTIXCLIENT_TITLEBARSTYLE_H