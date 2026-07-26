#ifndef BRIDGEPROCESS_H
#define BRIDGEPROCESS_H

#include <QObject>
#include <QJSValue>
#include <QString>

class AxScriptEngine;

class BridgeProcess : public QObject {
Q_OBJECT
    AxScriptEngine* scriptEngine = nullptr;

public:
    explicit BridgeProcess(AxScriptEngine* engine, QObject* parent = nullptr);

public Q_SLOTS:
    QJSValue exec(const QString& command, int timeoutMs = 30000) const;
    QString workdir() const;
    void set_workdir(const QString& path);

private:
    QString m_workdir;
};

#endif
