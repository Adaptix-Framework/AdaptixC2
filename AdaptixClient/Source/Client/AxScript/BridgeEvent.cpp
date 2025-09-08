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

    this->scriptEngine->registerEvent(event, handler, nullptr, list_agents, list_os, list_listeners, event_id);
}



void BridgeEvent::on_filebrowser_disks(const QJSValue &handler, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners, const QString &event_id) {
    this->reg("FileBroserDisks", "on_filebrowser_disks", handler, agents, os, listeners, event_id);
}

void BridgeEvent::on_filebrowser_list(const QJSValue &handler, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners, const QString &event_id) {
    this->reg("FileBroserList", "on_filebrowser_list", handler, agents, os, listeners, event_id);
}

void BridgeEvent::on_filebrowser_upload(const QJSValue &handler, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners, const QString &event_id) {
    this->reg("FileBrowserUpload", "on_filebrowser_upload", handler, agents, os, listeners, event_id);
}

void BridgeEvent::on_processbrowser_list(const QJSValue &handler, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners, const QString &event_id) {
    this->reg("ProcessBrowserList", "on_processbrowser_list", handler, agents, os, listeners, event_id);
}

void BridgeEvent::on_new_agent(const QJSValue &handler, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners, const QString &event_id) {
    this->reg("new_agent", "on_new_agent", handler, agents, os, listeners, event_id);
}

void BridgeEvent::on_ready(const QJSValue &handler, const QString &event_id)
{
    if (!handler.isCallable())
        emit scriptError("on_ready -> handler in not Callable");

    this->scriptEngine->registerEvent("ready", handler, nullptr, QSet<QString>(), QSet<QString>(), QSet<QString>(), event_id);
}

void BridgeEvent::on_disconnect(const QJSValue &handler, const QString &event_id)
{
    if (!handler.isCallable())
        emit scriptError("on_disconnect -> handler in not Callable");

    this->scriptEngine->registerEvent("disconnect", handler, nullptr, QSet<QString>(), QSet<QString>(), QSet<QString>(), event_id);
}

QString BridgeEvent::on_interval(const QJSValue &handler, int delay, QString event_id)
{
    if (!handler.isCallable())
        emit scriptError("on_timer -> handler in not Callable");

    if (delay < 0) delay = 0;
    if (event_id == "") event_id = "interval_" + GenerateRandomString(8, "hex");

    QTimer* timer = new QTimer(this);
    timer->setInterval(delay*1000);
    timer->setSingleShot(false);

    this->scriptEngine->registerEvent("timer", handler, timer, QSet<QString>(), QSet<QString>(), QSet<QString>(), event_id);

    connect(timer, &QTimer::timeout, this, [handler]() mutable { handler.call(); });

    timer->start();
    return event_id;
}

QString BridgeEvent::on_timeout(const QJSValue &handler, int delay, QString event_id)
{
    if (!handler.isCallable())
        emit scriptError("on_timeout -> handler in not Callable");

    if (delay < 0) delay = 0;
    if (event_id == "") event_id = "timeout_" + GenerateRandomString(8, "hex");

    QTimer* timer = new QTimer(this);
    timer->setInterval(delay*1000);
    timer->setSingleShot(true);

    this->scriptEngine->registerEvent("timer", handler, timer, QSet<QString>(), QSet<QString>(), QSet<QString>(), event_id);

    connect(timer, &QTimer::timeout, this, [this, event_id, handler]() mutable {
        handler.call();
        this->remove(event_id);
    });

    timer->start();
    return event_id;
}



QJSValue BridgeEvent::list()
{
    QStringList events = this->scriptEngine->manager()->EventList();
    QVariantList list;
    for (auto id : events)
        list.append(id);

    return this->scriptEngine->engine()->toScriptValue(list);
}

void BridgeEvent::remove(const QString &event_id) { this->scriptEngine->manager()->EventRemove(event_id); }
