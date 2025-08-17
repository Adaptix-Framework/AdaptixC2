#include <Client/AxScript/BridgeTimer.h>
#include <Client/AxScript/AxScriptEngine.h>

int BridgeTimer::nextTimerId = 1;

BridgeTimer::BridgeTimer(AxScriptEngine* scriptEngine, QObject* parent) 
    : QObject(parent), scriptEngine(scriptEngine)
{
}

BridgeTimer::~BridgeTimer() 
{
}

AxScriptEngine* BridgeTimer::GetScriptEngine() const 
{ 
    return this->scriptEngine; 
}

int BridgeTimer::setTimeout(const QJSValue& callback, int delay)
{
    if (!callback.isCallable()) {
        emit engineError("setTimeout: callback must be a function");
        return -1;
    }

    if (delay < 0) {
        delay = 0;
    }

    QTimer::singleShot(delay, [callback]() {
        if (callback.isCallable()) {
            callback.call();
        }
    });
    
    return nextTimerId++;
}

qintptr BridgeTimer::setInterval(const QJSValue& callback, int delay)
{
    if (!callback.isCallable()) {
        emit engineError("setInterval: callback must be a function");
        return -1;
    }

    if (delay < 0) {
        delay = 0;
    }

    QTimer* timer = new QTimer(this);
    timer->setInterval(delay);
    timer->setSingleShot(false);
    
    QObject::connect(timer, &QTimer::timeout, [callback]() {
        if (callback.isCallable()) {
            callback.call();
        }
    });
    
    timer->start();
    return reinterpret_cast<qintptr>(timer);
}

void BridgeTimer::clearTimeout(int timerId)
{
    // For setTimeout using QTimer::singleShot, clearing is not easily supported
    // This is a placeholder
}

void BridgeTimer::clearInterval(qintptr timerId)
{
    if (timerId > 0) {
        QTimer* timer = reinterpret_cast<QTimer*>(timerId);
        timer->stop();
        timer->deleteLater();
    }
}