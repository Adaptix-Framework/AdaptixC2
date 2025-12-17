#include <Client/AxScript/BridgeEvent.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/AxScriptUtils.h>

BridgeEvent::BridgeEvent(AxScriptEngine* scriptEngine, QObject* parent) : QObject(parent), scriptEngine(scriptEngine) {}

BridgeEvent::~BridgeEvent() {}

void BridgeEvent::reg(const QString &event, const QString &type, const QJSValue &handler, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners, const QString &event_id)
{
    if (!handler.isCallable()) {
        Q_EMIT scriptError(type + " -> handler is not Callable");
        return;
    }

    if (!AxScriptUtils::isValidNonEmptyArray(agents)) {
        Q_EMIT scriptError(type + " -> agents is undefined or empty");
        return;
    }

    QSet<QString> list_agents    = AxScriptUtils::jsArrayToStringSet(agents);
    QSet<QString> list_os        = AxScriptUtils::jsArrayToStringSet(os);
    QSet<QString> list_listeners = AxScriptUtils::jsArrayToStringSet(listeners);

    this->scriptEngine->registerEvent(event, handler, nullptr, list_agents, list_os, list_listeners, event_id);
}



void BridgeEvent::on_filebrowser_disks(const QJSValue &handler, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners, const QString &event_id) {
    this->reg("FileBrowserDisks", "on_filebrowser_disks", handler, agents, os, listeners, event_id);
}

void BridgeEvent::on_filebrowser_list(const QJSValue &handler, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners, const QString &event_id) {
    this->reg("FileBrowserList", "on_filebrowser_list", handler, agents, os, listeners, event_id);
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
    if (!handler.isCallable()) {
        Q_EMIT scriptError("on_ready -> handler is not Callable");
        return;
    }

    this->scriptEngine->registerEvent("ready", handler, nullptr, QSet<QString>(), QSet<QString>(), QSet<QString>(), event_id);
}

void BridgeEvent::on_disconnect(const QJSValue &handler, const QString &event_id)
{
    if (!handler.isCallable()) {
        Q_EMIT scriptError("on_disconnect -> handler is not Callable");
        return;
    }

    this->scriptEngine->registerEvent("disconnect", handler, nullptr, QSet<QString>(), QSet<QString>(), QSet<QString>(), event_id);
}

QString BridgeEvent::on_interval(const QJSValue &handler, int delay, QString event_id)
{
    if (!handler.isCallable()) {
        Q_EMIT scriptError("on_interval -> handler is not Callable");
        return "";
    }

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
    if (!handler.isCallable()) {
        Q_EMIT scriptError("on_timeout -> handler is not Callable");
        return "";
    }

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
