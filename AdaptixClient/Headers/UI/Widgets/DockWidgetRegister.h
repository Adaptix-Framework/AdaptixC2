#ifndef ADAPTIXCLIENT_DOCKWIDGETREGISTER_H
#define ADAPTIXCLIENT_DOCKWIDGETREGISTER_H

#include <QString>
#include <QMap>
#include <QList>

/**
 * @brief Registry for dock widgets with blink support.
 *
 * Widgets register themselves using the REGISTER_DOCK_WIDGET macro.
 * The registry is used to dynamically generate settings UI and
 * validate widget blink settings.
 */
class WidgetRegistry {
public:
    struct WidgetInfo {
        QString className;      // e.g., "ConsoleWidget"
        QString displayName;    // e.g., "Agent Console"
        bool    defaultState;
    };

    static WidgetRegistry& instance() {
        static WidgetRegistry registry;
        return registry;
    }

    void registerWidget(const QString& className, const QString& displayName, bool defaultState) {
        m_widgets[className] = {className, displayName, defaultState};
    }

    QList<WidgetInfo> widgets() const { return m_widgets.values(); }

private:
    WidgetRegistry() = default;
    QMap<QString, WidgetInfo> m_widgets;
};

/**
 * @brief Macro to register a dock widget in the registry.
 *
 * Usage in .cpp file:
 *   REGISTER_DOCK_WIDGET(ConsoleWidget, "Agent Console")
 */
#define REGISTER_DOCK_WIDGET(ClassName, DisplayName, DefaultState) \
    namespace { \
        struct ClassName##_Registrar { \
            ClassName##_Registrar() { \
                WidgetRegistry::instance().registerWidget(#ClassName, DisplayName, DefaultState); \
            } \
        } _##ClassName##_registrar; \
    }

#endif //ADAPTIXCLIENT_DOCKWIDGETREGISTER_H