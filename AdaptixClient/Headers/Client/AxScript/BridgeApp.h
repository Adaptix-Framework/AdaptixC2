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

public:
    explicit BridgeApp(AxScriptEngine* scriptEngine, QObject* parent = nullptr);
    ~BridgeApp() override;
    AxScriptEngine* GetScriptEngine() const;

public Q_SLOTS:
    QJSValue agents() const;
    QJSValue agent_info(const QString &id, const QString &property) const;
    void     agent_hide(const QJSValue& agents);
    void     agent_remove(const QJSValue& agents);
    void     agent_set_color(const QJSValue& agents, const QString &background, const QString &foreground, const bool reset);
    void     agent_set_impersonate(const QString &id, const QString &impersonate, const bool elevated);
    void     agent_set_mark(const QJSValue& agents, const QString &mark);
    void     agent_set_tag(const QJSValue& agents, const QString &tag);
    void     agent_update_data(const QString &id, const QJSValue &data);
    void     agent_set_command_group(const QString &id, const QString &group, bool enabled);
    QJSValue agent_get_command_groups(const QString &id) const;
    QString  arch(const QString &id) const;
    QString  bof_pack(const QString &types, const QJSValue &args);
    void     copy_to_clipboard(const QString &text);
    // Code conversion (language: "c", "csharp", "python", "golang", "vbs", "nim", "rust", "powershell").
    QString  convert_to_code(const QString &language, const QByteArray &data, const QString &varName = "shellcode") const;
    void     console_message(const QString &id, const QString &message, const QString &type = "", const QString &text = "");
    QJSValue credentials() const;
    void     credentials_add(const QString &username, const QString &password, const QString &realm = "", const QString &type = "password", const QString &tag = "", const QString &storage = "manual", const QString &host = "");
    void     credentials_add_list(const QVariantList &array);
    QObject* create_command(const QString &name, const QString &description, const QString &example = "", const QString &message = "");
    QObject* create_commands_group(const QString &name, const QJSValue& array);
    QJSValue downloads() const;
    // (algorithm: "hex", "base64", "base32", "zip", "xor")
    // encode_data / encode_file: `data` may be ArrayBuffer / Uint8Array / string. Return shape depends on alg:
    //   - text-format algs ("hex" / "base64" / "base32") return a JS string
    //   - binary-format algs ("xor" / "zip")             return ArrayBuffer
    // decode_data / decode_file: input shape depends on alg:
    //   - text-format algs accept the encoded string
    //   - binary-format algs accept ArrayBuffer / Uint8Array / string-as-bytes
    QVariant   encode_data(const QString &algorithm, const QByteArray &data, const QString &key = QString()) const;
    QVariant   encode_file(const QString &algorithm, const QString &path, const QString &key = QString()) const;
    QByteArray decode_data(const QString &algorithm, const QByteArray &data, const QString &key = QString()) const;
    QByteArray decode_file(const QString &algorithm, const QString &path, const QString &key = QString()) const;
    void     execute_alias(const QString &id, const QString &cmdline, const QString &command, const QString &message = "", const QJSValue &hook = QJSValue(), const QJSValue &handler = QJSValue()) const;
    void     execute_alias_hook(const QString &id, const QString &cmdline, const QString &command, const QString &message, const QJSValue &hook) const;
    void     execute_alias_handler(const QString &id, const QString &cmdline, const QString &command, const QString &message, const QJSValue &handler) const;
    void     execute_browser(const QString &id, const QString &command) const;
    void     execute_command(const QString &id, const QString &command, const QJSValue &hook = QJSValue(), const QJSValue &handler = QJSValue()) const;
    void     execute_command_hook(const QString &id, const QString &command, const QJSValue &hook) const;
    void     execute_command_handler(const QString &id, const QString &command, const QJSValue &handler) const;
    QString  file_basename(const QString &path) const;
    QString  file_dirname(const QString &path) const;
    QString  file_extension(const QString &path) const;
    bool     file_exists(const QString &path) const;
    QByteArray file_read(QString path) const;
    qint64     file_size(const QString &path) const;
    bool       file_write(QString path, const QByteArray &data, bool append = false) const;
    QString  format_size(const qint64 &size) const;
    QString  format_time(const QString &format, const int &time) const;
    QJSValue get_commands(const QString &id) const;
    QString  get_project() const;
    QString  hash(const QString &algorithm, int length, const QByteArray &input);
    QJSValue ids() const;
    QJSValue interfaces() const;
    bool     is64(const QString &id) const;
    bool     isactive(const QString &id) const;
    bool     isadmin(const QString &id) const;
    void     log(const QString &text);
    void     log_error(const QString &text);
    void     open_agent_console(const QString &id);
    void     open_access_tunnel(const QString &id, bool socks4, bool socks5, bool lportfwd, bool rportfwd);
    void     open_browser_files(const QString &id, const QJSValue &zone = QJSValue());
    void     open_browser_process(const QString &id, const QJSValue &zone = QJSValue());
    void     open_remote_terminal(const QString &id, const QJSValue &zone = QJSValue());
    void     open_remote_shell(const QString &id, const QJSValue &zone = QJSValue());
    void     open_code_editor(const QJSValue& arg1 = QJSValue(), const QJSValue& arg2 = QJSValue());
    QString  editor_profile_upsert(const QJSValue& spec);
    bool     prompt_confirm(const QString &title, const QString &text);
    QString  prompt_open_file(const QString &caption = "Select file", const QString &filter = QString());
    QString  prompt_open_dir(const QString &caption = "Select directory");
    QString  prompt_save_file(const QString &filename, const QString &caption = "Select file", const QString &filter = QString());
    QString  random_string(const int length, const QString &setname);
    int      random_int(const int min, const int max);
    void     register_commands_group(QObject* obj, const QJSValue& agents, const QJSValue& os, const QJSValue& listeners);
    void     register_service_commands(QObject* obj);
    void     script_import(const QString &path);
    void     script_load(const QString &path);
    void     script_unload(const QString &path);
    QString  script_dir();
    bool     event_handler_register(const QJSValue& meta);
    QJSValue screenshots();
    void     plugin_service_command(const QString &service, const QString &command, const QJSValue &args = QJSValue());
    QJSValue plugin_service_wait(const QString &service, const QString &command, const QJSValue &args = QJSValue(), const QJSValue &timeoutMs = QJSValue());
    void     plugin_agent_command(const QJSValue &agentId, const QString &command, const QJSValue &args = QJSValue());
    void     plugin_listener_command(const QString &listenerName, const QString &command, const QJSValue &args = QJSValue());
    void     show_message(const QString &title, const QString &text);
    QJSValue targets() const;
    void     targets_add(const QString &computer, const QString &domain, const QString &address, const QString &os = "unknown", const QString &osDesc = "", const QString &tag = "", const QString &info = "", bool alive = true);
    void     targets_add_list(const QVariantList &array);
    QJSValue payloads() const;
    int      ticks();
    QStringList tokenize(const QString &cmdline) const;
    QJSValue tunnels();
    QJSValue validate_command(const QString &id, const QString &command) const;

Q_SIGNALS:
    void consoleMessage(const QString &msg);
    void consoleError(const QString &msg);
    void engineError(const QString &msg);
};

#endif
