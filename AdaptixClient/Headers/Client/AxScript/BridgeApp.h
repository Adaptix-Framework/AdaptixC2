#ifndef BRIDGEAPP_H
#define BRIDGEAPP_H

#include <QObject>
#include <QString>
#include <QJSValue>

class AxScriptEngine;
class Command;

class BridgeApp : public QObject {
Q_OBJECT
    AxScriptEngine* scriptEngine;
    QWidget*        widget;

public:
    explicit BridgeApp(AxScriptEngine* scriptEngine, QObject* parent = nullptr);
    ~BridgeApp() override;
    AxScriptEngine* GetScriptEngine() const;

public slots:
    QJSValue agents() const;
    QJSValue agent_info(const QString &id, const QString &property) const;
    QString  arch(const QString &id) const;
    QString  bof_pack(const QString &types, const QJSValue &args);
    void     console_message(const QString &id, const QString &message, const QString &type = "", const QString &text = "");
    void     credentials_add(const QString &username, const QString &password, const QString &realm = "", const QString &type = "password", const QString &tag = "", const QString &storage = "manual", const QString &host = "");
    QObject* create_command(const QString &name, const QString &description, const QString &example = "", const QString &message = "");
    QObject* create_commands_group(const QString &name, const QJSValue& array);
    void     execute_alias(const QString &id, const QString &cmdline, const QString &command, const QString &message = "", const QJSValue &hook = QJSValue()) const;
    void     execute_command(const QString &id, const QString &command, const QJSValue &hook = QJSValue()) const;
    QString  file_basename(const QString &path) const;
    bool     file_exists(const QString &path) const;
    QString  file_read(QString path) const;
    QString  format_time(const QString &format, const int &time) const;
    bool     is64(const QString &id) const;
    bool     isadmin(const QString &id) const;
    void     log(const QString &text);
    void     log_error(const QString &text);
    void     open_access_tunnel(const QString &id, bool socks4, bool socks5, bool lportfwd, bool rportfwd);
    void     open_browser_files(const QString &id);
    void     open_browser_process(const QString &id);
    void     open_browser_terminal(const QString &id);
    void     register_commands_group(QObject* obj, const QJSValue& agents, const QJSValue& os, const QJSValue& listeners);
    void     script_load(const QString &path);
    void     script_unload(const QString &path);
    QString  script_dir();
    int      ticks();

signals:
    void consoleMessage(const QString &msg);
    void consoleError(const QString &msg);
    void engineError(const QString &msg);
};

#endif
