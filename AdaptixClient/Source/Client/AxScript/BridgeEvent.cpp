#include <Client/AxScript/BridgeEvent.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/AxScriptManager.h>

BridgeEvent::BridgeEvent(AxScriptEngine* scriptEngine, QObject* parent) : QObject(parent), scriptEngine(scriptEngine) {}

BridgeEvent::~BridgeEvent() {}

void BridgeEvent::reg(const QString &event, const QString &type, const QJSValue &handler, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners, const QString &event_id)
{
    if (!handler.isCallable()) {
        emit scriptError( type + " -> handler in not Callable");
        return;
    }

    QSet<QString> list_agents;
    QSet<QString> list_os;
    QSet<QString> list_listeners;

    if (agents.isUndefined() || agents.isNull() || !agents.isArray() || agents.property("length").toInt() == 0) {
        emit scriptError(type + " -> agents in undefined");
        return;
    }

    for (int i = 0; i < agents.property("length").toInt(); ++i) {
        QJSValue val = agents.property(i);
        list_agents.insert(val.toString());
    }

    if (!os.isUndefined() && !os.isNull() && os.isArray()) {
        for (int i = 0; i < os.property("length").toInt(); ++i) {
            QJSValue val = os.property(i);
            list_os << val.toString();
        }
    }

    if (!listeners.isUndefined() && !listeners.isNull() && listeners.isArray()) {
        for (int i = 0; i < listeners.property("length").toInt(); ++i) {
            QJSValue val = listeners.property(i);
            list_listeners << val.toString();
        }
    }

    this->scriptEngine->registerEvent(event, handler, list_agents, list_os, list_listeners, event_id);
}

void BridgeEvent::on_new_agent(const QJSValue &handler, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners, const QString &event_id) {
    this->reg("new_agent", "on_new_agent", handler, agents, os, listeners, event_id);
}


void BridgeEvent::on_filebrowser_disks(const QJSValue &handler, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners, const QString &event_id) {
    this->reg("FileBroserDisks", "on_filebrowser_disks", handler, agents, os, listeners, event_id);
}

void BridgeEvent::on_filebrowser_list(const QJSValue &handler, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners, const QString &event_id) {
    this->reg("FileBroserList", "on_filebrowser_list", handler, agents, os, listeners, event_id);
}

void BridgeEvent::on_filebrowser_upload(const QJSValue &handler, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners, const QString &event_id) {
    this->reg("FileBroserUpload", "on_filebrowser_upload", handler, agents, os, listeners, event_id);
}

void BridgeEvent::on_processbrowser_list(const QJSValue &handler, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners, const QString &event_id) {
    this->reg("ProcessBrowserList", "on_processbrowser_list", handler, agents, os, listeners, event_id);
}

void BridgeEvent::remove(const QString &event_id) { this->scriptEngine->manager()->RemoveEvent(event_id); }
