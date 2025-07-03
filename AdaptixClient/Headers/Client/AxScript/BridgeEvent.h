#ifndef BRIDGEEVENT_H
#define BRIDGEEVENT_H

#include <QJSEngine>
#include <QJSValue>
#include <QList>
#include <QWidget>

class AxScriptEngine;

class BridgeEvent : public QObject {
Q_OBJECT
    AxScriptEngine* scriptEngine;
    QWidget*        widget;
    QList<QJSValue> eventHandlers;

public:
    BridgeEvent(AxScriptEngine* scriptEngine, QObject* parent = nullptr);
    ~BridgeEvent() override;

public slots:
    void on_event(const QJSValue &handler);
    void emitEventTestClick();

signals:
    void consoleAppendError(const QString &msg);
};

#endif //BRIDGEEVENT_H
