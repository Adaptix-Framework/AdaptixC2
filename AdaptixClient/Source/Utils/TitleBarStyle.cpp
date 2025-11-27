#include <Utils/TitleBarStyle.h>
#include <QFile>
#include <QRegularExpression>
#include <QGuiApplication>

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#endif

namespace TitleBarStyle {

    static bool isLightTheme(const QString& themeName)
    {
        QString themeFile = ":/themes/" + themeName;
        QFile file(themeFile);

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;

        QString content = QString::fromUtf8(file.read(500));
        file.close();

        QRegularExpression regex(R"(@theme-type:\s*(light|dark))");
        QRegularExpressionMatch match = regex.match(content);

        if (match.hasMatch())
            return match.captured(1).toLower() == "light";

        return false;
    }

    void applyForTheme(QWidget* window, const QString& themeName)
    {
        bool light = isLightTheme(themeName);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        QGuiApplication::styleHints()->setColorScheme(
            light ? Qt::ColorScheme::Light : Qt::ColorScheme::Dark
        );
        Q_UNUSED(window)
#elif defined(Q_OS_WIN)
        if (!window) return;
        HWND hwnd = reinterpret_cast<HWND>(window->winId());
        BOOL darkMode = light ? FALSE : TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
#else
        Q_UNUSED(window)
        Q_UNUSED(light)
#endif
    }
}