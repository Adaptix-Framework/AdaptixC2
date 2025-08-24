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

public:
    explicit BridgeEvent(AxScriptEngine* scriptEngine, QObject* parent = nullptr);
    ~BridgeEvent() override;

    void reg(const QString &event, const QString &type, const QJSValue &handler, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners, const QString &event_id);

public slots:
    void on_filebrowser_disks(const QJSValue &handler, const QJSValue &agents, const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue(), const QString &event_id = "");
    void on_filebrowser_list(const QJSValue &handler, const QJSValue &agents, const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue(), const QString &event_id = "");
    void on_filebrowser_upload(const QJSValue &handler, const QJSValue &agents, const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue(), const QString &event_id = "");
    void on_processbrowser_list(const QJSValue &handler, const QJSValue &agents, const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue(), const QString &event_id = "");

    void on_new_agent(const QJSValue &handler, const QJSValue &agents, const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue(), const QString &event_id = "");

    void on_disconnect(const QJSValue &handler, const QString &event_id = "");
    void on_ready(const QJSValue &handler, const QString &event_id = "");

    QString on_interval(const QJSValue &handler, int delay, QString event_id = "");
    QString on_timeout(const QJSValue &handler, int delay, QString event_id = "");

    QJSValue list();
    void     remove(const QString &event_id);

signals:
    void scriptError(const QString &msg);
};

#endif
