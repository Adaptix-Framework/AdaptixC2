#ifndef BRIDGETIMER_H
#define BRIDGETIMER_H

#include <QObject>
#include <QString>
#include <QJSValue>
#include <QTimer>

class AxScriptEngine;

class BridgeTimer : public QObject {
Q_OBJECT
    AxScriptEngine* scriptEngine;
    static int nextTimerId;

public:
    explicit BridgeTimer(AxScriptEngine* scriptEngine, QObject* parent = nullptr);
    ~BridgeTimer() override;
    AxScriptEngine* GetScriptEngine() const;

public slots:
    int setTimeout(const QJSValue& callback, int delay);
    qintptr setInterval(const QJSValue& callback, int delay);
    void clearTimeout(int timerId);
    void clearInterval(qintptr timerId);

signals:
    void engineError(const QString& message);
};

#endif