#ifndef ADAPTIXCLIENT_WIDGETREGISTRY_H
#define ADAPTIXCLIENT_WIDGETREGISTRY_H

#include <QString>
#include <QMap>
#include <QList>

/**
 * @brief Registry for dock widgets with notification support.
 * 
 * Widgets register themselves using the REGISTER_DOCK_WIDGET macro.
 * The registry is used to dynamically generate settings UI and
 * validate widget notification settings.
 */
class WidgetRegistry {
public:
    struct WidgetInfo {
        QString className;      // e.g., "ConsoleWidget"
        QString displayName;    // e.g., "Agent Console"
    };
    
    static WidgetRegistry& instance() {
        static WidgetRegistry registry;
        return registry;
    }
    
    void registerWidget(const QString& className, const QString& displayName) {
        m_widgets[className] = {className, displayName};
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
#define REGISTER_DOCK_WIDGET(ClassName, DisplayName) \
    namespace { \
        struct ClassName##_Registrar { \
            ClassName##_Registrar() { \
                WidgetRegistry::instance().registerWidget(#ClassName, DisplayName); \
            } \
        } _##ClassName##_registrar; \
    }

#endif // ADAPTIXCLIENT_WIDGETREGISTRY_H
