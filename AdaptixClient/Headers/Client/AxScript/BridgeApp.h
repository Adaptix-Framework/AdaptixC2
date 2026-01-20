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
    QString  arch(const QString &id) const;
    QString  bof_pack(const QString &types, const QJSValue &args);
    void     copy_to_clipboard(const QString &text);
    // Code conversion (language: "c", "csharp", "python", "golang", "vbs", "nim", "rust", "powershell")
    QString  convert_to_code(const QString &language, const QString &base64Data, const QString &varName = "shellcode") const;
    void     console_message(const QString &id, const QString &message, const QString &type = "", const QString &text = "");
    QJSValue credentials() const;
    void     credentials_add(const QString &username, const QString &password, const QString &realm = "", const QString &type = "password", const QString &tag = "", const QString &storage = "manual", const QString &host = "");
    void     credentials_add_list(const QVariantList &array);
    QObject* create_command(const QString &name, const QString &description, const QString &example = "", const QString &message = "");
    QObject* create_commands_group(const QString &name, const QJSValue& array);
    QJSValue downloads() const;
    QString  donut_generate(
        const QString &file, 
        const QString &params, 
        const QString &arch, 
        const QString &pipeName = "", 
        const QString &stubBase64 = "", 
        int compress = 1, 
        int entropy = 3, 
        int exit_opt = 2, 
        int bypass = 3, 
        int headers = 1
    );
    // (algorithm: "hex", "base64", "base32", "zip", "xor")
    QString  decode_data(const QString &algorithm, const QString &data, const QString &key = QString()) const;
    QString  decode_file(const QString &algorithm, const QString &path, const QString &key = QString()) const;
    QString  encode_data(const QString &algorithm, const QString &data, const QString &key = QString()) const;
    QString  encode_file(const QString &algorithm, const QString &path, const QString &key = QString()) const;
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
    QString  file_read(QString path) const;
    qint64   file_size(const QString &path) const;
    bool     file_write_text(QString path, const QString &content, bool append = false) const;
    bool     file_write_binary(QString path, const QString &base64Content) const;
    QString  format_size(const int &size) const;
    QString  format_time(const QString &format, const int &time) const;
    QJSValue get_commands(const QString &id) const;
    QString  hash(const QString &algorithm, int length, const QString &input);
    QJSValue ids() const;
    QJSValue interfaces() const;
    bool     is64(const QString &id) const;
    bool     isactive(const QString &id) const;
    bool     isadmin(const QString &id) const;
    void     log(const QString &text);
    void     log_error(const QString &text);
    void     open_agent_console(const QString &id);
    void     open_access_tunnel(const QString &id, bool socks4, bool socks5, bool lportfwd, bool rportfwd);
    void     open_browser_files(const QString &id);
    void     open_browser_process(const QString &id);
    void     open_remote_terminal(const QString &id);
    void     open_remote_shell(const QString &id);
    bool     prompt_confirm(const QString &title, const QString &text);
    QString  prompt_open_file(const QString &caption = "Select file", const QString &filter = QString());
    QString  prompt_open_dir(const QString &caption = "Select directory");
    QString  prompt_save_file(const QString &filename, const QString &caption = "Select file", const QString &filter = QString());
    QString  random_string(const int length, const QString &setname);
    int      random_int(const int min, const int max);
    void     register_commands_group(QObject* obj, const QJSValue& agents, const QJSValue& os, const QJSValue& listeners);
    void     script_import(const QString &path);
    void     script_load(const QString &path);
    void     script_unload(const QString &path);
    QString  script_dir();
    QJSValue screenshots();
    void     service_command(const QString &service, const QString &command, const QJSValue &args = QJSValue());
    void     show_message(const QString &title, const QString &text);
    QJSValue targets() const;
    void     targets_add(const QString &computer, const QString &domain, const QString &address, const QString &os = "unknown", const QString &osDesc = "", const QString &tag = "", const QString &info = "", bool alive = true);
    void     targets_add_list(const QVariantList &array);
    int      ticks();
    QJSValue tunnels();
    QJSValue validate_command(const QString &id, const QString &command) const;

Q_SIGNALS:
    void consoleMessage(const QString &msg);
    void consoleError(const QString &msg);
    void engineError(const QString &msg);
};

#endif
