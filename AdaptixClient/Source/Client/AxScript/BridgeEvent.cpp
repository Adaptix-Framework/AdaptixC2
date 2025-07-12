#include <Client/AxScript/BridgeEvent.h>

BridgeEvent::BridgeEvent(AxScriptEngine* scriptEngine, QObject* parent) : QObject(parent), scriptEngine(scriptEngine), widget(new QWidget()) {}

BridgeEvent::~BridgeEvent() { delete widget; }

void BridgeEvent::on_event(const QJSValue &handler)
{
    if (handler.isCallable())
        eventHandlers.append(handler);
    else
        emit scriptError("on_event -> handler in not Callable");
}

void BridgeEvent::emitEventTestClick()
{
    for (const auto& handler : eventHandlers) {
        if (handler.isCallable())
            eventHandlers.append(handler);
        else
            emit scriptError("EventTestClick -> handler in not Callable");
    }
}
