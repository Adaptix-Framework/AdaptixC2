#include <main.h>
#include <QTimeZone>
#include <Agent/Agent.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/BridgeApp.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/AxCommandWrappers.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/AxScriptUtils.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/CredentialsWidget.h>
#include <UI/Widgets/TargetsWidget.h>

BridgeApp::BridgeApp(AxScriptEngine* scriptEngine, QObject* parent) : QObject(parent), scriptEngine(scriptEngine) {}

BridgeApp::~BridgeApp() = default;

AxScriptEngine* BridgeApp::GetScriptEngine() const { return this->scriptEngine; }



QJSValue BridgeApp::agents() const
{
    QVariantMap list;
    auto mapAgents = scriptEngine->manager()->GetAgents();

    for (auto agent : mapAgents) {
        QVariantMap map;
        map["id"]           = agent->data.Id;
        map["type"]         = agent->data.Name;
        map["listener"]     = agent->data.Listener;
        map["external_ip"]  = agent->data.ExternalIP;
        map["internal_ip"]  = agent->data.InternalIP;
        map["domain"]       = agent->data.Domain;
        map["computer"]     = agent->data.Computer;
        map["username"]     = agent->data.Username;
        map["impersonated"] = agent->data.Impersonated;
        map["process"]      = agent->data.Process;
        map["arch"]         = agent->data.Arch;
        map["pid"]          = agent->data.Pid.toInt();
        map["tid"]          = agent->data.Tid.toInt();
        map["gmt"]          = agent->data.GmtOffset;
        map["elevated"]     = agent->data.Elevated;
        map["tags"]         = agent->data.Tags;
        map["async"]        = agent->data.Async;
        map["sleep"]        = agent->data.Sleep;
        map["os_full"]      = agent->data.OsDesc;

        if (agent->data.Os == OS_WINDOWS)    map["os"] = "windows";
        else if (agent->data.Os == OS_LINUX) map["os"] = "linux";
        else if (agent->data.Os == OS_MAC)   map["os"] = "macos";
        else                                 map["os"] = "unknown";

        list[agent->data.Id] = map;
    }

    return this->scriptEngine->engine()->toScriptValue(list);
}

QJSValue BridgeApp::agent_info(const QString &id, const QString &property) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id))
        return false;

    QJSValue ret;
    auto info = mapAgents[id]->data;

    if (property == "id")
        return QJSValue(info.Id);
    if (property == "type")
        return QJSValue(info.Name);
    if (property == "listener")
        return QJSValue(info.Listener);
    if (property == "externalIP")
        return QJSValue(info.ExternalIP);
    if (property == "internalIP")
        return QJSValue(info.InternalIP);
    if (property == "domain")
        return QJSValue(info.Domain);
    if (property == "computer")
        return QJSValue(info.Computer);
    if (property == "username")
        return QJSValue(info.Username);
    if (property == "impersonated")
        return QJSValue(info.Impersonated);
    if (property == "process")
        return QJSValue(info.Process);
    if (property == "arch")
        return QJSValue(info.Arch);
    if (property == "pid")
        return QJSValue(info.Pid.toInt());
    if (property == "tid")
        return QJSValue(info.Tid.toInt());
    if (property == "gmt")
        return QJSValue(info.GmtOffset);
    if (property == "elevated")
        return QJSValue(info.Elevated);
    if (property == "tags")
        return QJSValue(info.Tags);
    if (property == "async")
        return QJSValue(info.Async);
    if (property == "sleep")
        return QJSValue(info.Sleep);
    if (property == "os_full")
        return QJSValue(info.OsDesc);
    if (property == "os") {
        if (info.Os == OS_WINDOWS) return "windows";
        else if (info.Os == OS_LINUX) return "linux";
        else if (info.Os == OS_MAC) return "macos";
        else return "unknown";
    }

    return QJSValue::UndefinedValue;
}

void BridgeApp::agent_hide(const QJSValue &agents)
{
    if (!AxScriptUtils::isValidArray(agents)) {
        Q_EMIT engineError("agent_hide expected array of strings in agents parameter!");
        return;
    }
    scriptEngine->manager()->AppAgentHide(AxScriptUtils::jsArrayToStringList(agents));
}

void BridgeApp::agent_remove(const QJSValue &agents)
{
    if (!AxScriptUtils::isValidArray(agents)) {
        Q_EMIT engineError("agent_remove expected array of strings in agents parameter!");
        return;
    }

    scriptEngine->manager()->AppAgentRemove(AxScriptUtils::jsArrayToStringList(agents));
}

void BridgeApp::agent_set_color(const QJSValue &agents, const QString &background, const QString &foreground, const bool reset)
{
    if (!AxScriptUtils::isValidArray(agents)) {
        Q_EMIT engineError("agent_set_color expected array of strings in agents parameter!");
        return;
    }
    scriptEngine->manager()->AppAgentSetColor(AxScriptUtils::jsArrayToStringList(agents), background, foreground, reset);
}

void BridgeApp::agent_set_impersonate(const QString &id, const QString &impersonate, const bool elevated) { scriptEngine->manager()->AppAgentSetImpersonate(id, impersonate, elevated); }

void BridgeApp::agent_set_mark(const QJSValue &agents, const QString &mark)
{
    if (!AxScriptUtils::isValidArray(agents)) {
        Q_EMIT engineError("agent_set_mark expected array of strings in agents parameter!");
        return;
    }

    scriptEngine->manager()->AppAgentSetMark(AxScriptUtils::jsArrayToStringList(agents), mark);
}

void BridgeApp::agent_set_tag(const QJSValue &agents, const QString &tag)
{
    if (!AxScriptUtils::isValidArray(agents)) {
        Q_EMIT engineError("agent_set_tag expected array of strings in agents parameter!");
        return;
    }

    scriptEngine->manager()->AppAgentSetTag(AxScriptUtils::jsArrayToStringList(agents), tag);
}

QString BridgeApp::arch(const QString &id) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id))
        return "x86";

    return mapAgents[id]->data.Arch;
}

QString BridgeApp::bof_pack(const QString &types, const QJSValue &args)
{
    if (!args.isArray()) {
        Q_EMIT engineError("bof_pack expected array of arguments!");
        return "";
    }

    QStringList items = types.split(",", Qt::SkipEmptyParts);
    int length = args.property("length").toInt();

    if (items.size() != length) {
        Q_EMIT engineError("bof_pack expects the same number of types and arguments!");
        return "";
    }

    QByteArray data;

    for (int i = 0; i < length; ++i) {
        QVariant value = args.property(i).toVariant();

        if (items[i] == "cstr") {
            if (!value.canConvert<QString>()) {
                Q_EMIT engineError(QString("bof_pack cannot convert argument at index %1 to string").arg(i));
                return "";
            }

            QByteArray valueData = value.toString().toUtf8();
            int strLength = valueData.size() + 1;

            QByteArray valueLengthData;
            valueLengthData.append(reinterpret_cast<const char*>(&strLength), 4);
            data.append(valueLengthData);

            valueData.append('\0');
            data.append(valueData);
        }
        else if (items[i] == "wstr") {
            if (!value.canConvert<QString>()) {
                Q_EMIT engineError(QString("bof_pack cannot convert argument at index %1 to string").arg(i));
                return "";
            }

            QString str = value.toString();
            const char16_t* utf16Data = reinterpret_cast<const char16_t*>(str.utf16());
            int utf16Length = str.size() + 1;

            QByteArray strData;
            strData.append(reinterpret_cast<const char*>(utf16Data), utf16Length * sizeof(char16_t));

            QByteArray strLengthData;
            int strLength = utf16Length * sizeof(char16_t);
            strLengthData.append(reinterpret_cast<const char*>(&strLength), 4);

            data.append(strLengthData);
            data.append(strData);
        }
        else if (items[i] == "bytes") {
            if (!value.canConvert<QString>()) {
                Q_EMIT engineError(QString("bof_pack cannot convert argument at index %1 to string").arg(i));
                return "";
            }

            QByteArray valueData = QByteArray::fromBase64(value.toString().toUtf8());
            int strLength = valueData.size();

            QByteArray valueLengthData;
            valueLengthData.append(reinterpret_cast<const char*>(&strLength), 4);
            data.append(valueLengthData);
            data.append(valueData);
        }
        else if (items[i] == "int") {
            if (!value.canConvert<int>()) {
                Q_EMIT engineError(QString("bof_pack cannot convert argument at index %1 to int").arg(i));
                return "";
            }

            int num = value.toInt();
            QByteArray numData;
            numData.append(reinterpret_cast<const char*>(&num), sizeof(num));
            data.append(numData);
        }
        else if (items[i] == "short") {
            if (!value.canConvert<int>()) {
                Q_EMIT engineError(QString("bof_pack cannot convert argument at index %1 to short").arg(i));
                return "";
            }

            short num  = static_cast<short>(value.toInt());
            QByteArray numData;
            numData.append(reinterpret_cast<const char*>(&num), sizeof(num));
            data.append(numData);
        }
        else {
            Q_EMIT engineError(QString("bof_pack does not expect type '%1' (index %2)").arg(items[i]).arg(i));
            return "";
        }
    }

    QByteArray strLengthData;
    int strLength = data.size();
    strLengthData.append(reinterpret_cast<const char*>(&strLength), sizeof(strLength));

    strLengthData.append(data);
    return strLengthData.toBase64();
}

void BridgeApp::copy_to_clipboard(const QString &text) { QApplication::clipboard()->setText(text); }

void BridgeApp::console_message(const QString &id, const QString &message, const QString &type, const QString &text)
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id))
        return;

    auto agent = mapAgents[id];
    if (!agent)
        return;

    int msgType = CONSOLE_OUT;
    if (type == "info")
        msgType = CONSOLE_OUT_LOCAL_INFO;
    else if (type == "success")
        msgType = CONSOLE_OUT_LOCAL_SUCCESS;
    else if (type == "error")
        msgType = CONSOLE_OUT_LOCAL_ERROR;

    agent->Console->ConsoleOutputMessage(QDateTime::currentSecsSinceEpoch(), "", msgType, message, text, false);
}

QJSValue BridgeApp::credentials() const
{
    QVariantMap list;
    auto vecCreds = scriptEngine->manager()->GetCredentials();

    for (auto cred : vecCreds) {
        QVariantMap map;
        map["id"]       = cred.CredId;
        map["username"] = cred.Username;
        map["password"] = cred.Password;
        map["realm"]    = cred.Realm;
        map["type"]     = cred.Type;
        map["tag"]      = cred.Tag;
        map["date"]     = cred.Date;
        map["storage"]  = cred.Storage;
        map["agent_id"] = cred.AgentId;
        map["host"]     = cred.Host;

        list[cred.CredId] = map;
    }

    return this->scriptEngine->engine()->toScriptValue(list);
}

void BridgeApp::credentials_add(const QString &username, const QString &password, const QString &realm, const QString &type, const QString &tag, const QString &storage, const QString &host)
{
    CredentialData cred = {"", username, password, realm, type, tag, "", 0, storage, "", host};

    QList<CredentialData> credsList;
    credsList.append(cred);
    scriptEngine->manager()->GetAdaptix()->CredentialsDock->CredentialsAdd(credsList);
}

void BridgeApp::credentials_add_list(const QVariantList &array)
{
    QList<CredentialData> credsList;
    for (const QVariant &item : array) {
        QVariantMap map = item.toMap();
        CredentialData cd = {};
        if (map.contains("username")) cd.Username = map["username"].toString();
        if (map.contains("password")) cd.Password = map["password"].toString();
        if (map.contains("realm"))    cd.Realm    = map["realm"].toString();
        if (map.contains("type"))     cd.Type     = map["type"].toString();
        if (map.contains("tag"))      cd.Tag      = map["tag"].toString();
        if (map.contains("storage"))  cd.Storage  = map["storage"].toString();
        if (map.contains("host"))     cd.Host     = map["host"].toString();
        credsList.append(cd);
    }

    if (credsList.isEmpty())
        return;

    scriptEngine->manager()->GetAdaptix()->CredentialsDock->CredentialsAdd(credsList);
}

QObject* BridgeApp::create_command(const QString &name, const QString &description, const QString &example, const QString &message)
{
    auto* wrapper = new AxCommandWrappers(name, description, example, message, this);
    connect(wrapper, &AxCommandWrappers::scriptError, this, &BridgeApp::engineError);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeApp::create_commands_group(const QString &name, const QJSValue &array)
{
    auto* wrapper = new AxCommandGroupWrapper(scriptEngine->engine(), this);
    connect(wrapper, &AxCommandGroupWrapper::scriptError, this, &BridgeApp::engineError);
    wrapper->SetParams(name, array);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QJSValue BridgeApp::downloads() const
{
    QVariantMap list;
    auto mapDownloads = scriptEngine->manager()->GetDownloads();

    for (auto download : mapDownloads) {
        QVariantMap map;
        map["id"]         = download.FileId;
        map["agent_id"]   = download.AgentId;
        map["agent_name"] = download.AgentName;
        map["user"]       = download.User;
        map["computer"]   = download.Computer;
        map["filename"]   = download.Filename;
        map["recv_size"]  = download.RecvSize;
        map["total_size"] = download.TotalSize;
        map["date"]       = download.Date;

        if (download.State == DOWNLOAD_STATE_RUNNING)
            map["state"] = "running";
        else if (download.State == DOWNLOAD_STATE_STOPPED)
            map["state"] = "stopped";
        else if (download.State == DOWNLOAD_STATE_FINISHED)
            map["state"] = "finished";
        else
            map["state"] = "canceled";

        list[download.FileId] = map;
    }

    return this->scriptEngine->engine()->toScriptValue(list);
}

void BridgeApp::execute_alias(const QString &id, const QString &cmdline, const QString &command, const QString &message, const QJSValue &hook) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id))
        return;

    auto agent = mapAgents[id];
    if (!agent)
        return;

    auto cmdResult = agent->commander->ProcessInput(id, command);
    if (!cmdResult.is_pre_hook) {
        if (!message.isEmpty())
            cmdResult.data["message"] = message;

        if (!hook.isUndefined() && !hook.isNull() && hook.isCallable())
            cmdResult.post_hook = {true, scriptEngine->context.name, hook};

        agent->Console->ProcessCmdResult(cmdline, cmdResult, false);
    }
}

void BridgeApp::execute_browser(const QString &id, const QString &command) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id))
        return;

    auto agent = mapAgents[id];
    if (!agent)
        return;

    auto cmdResult = agent->commander->ProcessInput(id, command);
    agent->Console->ProcessCmdResult(command, cmdResult, true);
}

void BridgeApp::execute_command(const QString &id, const QString &command, const QJSValue &hook) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id))
        return;

    auto agent = mapAgents[id];
    if (!agent)
        return;

    auto cmdResult = agent->commander->ProcessInput(id, command);
    if (!cmdResult.is_pre_hook) {

        if (!hook.isUndefined() && !hook.isNull() && hook.isCallable())
            cmdResult.post_hook = {true, scriptEngine->context.name, hook};

        agent->Console->ProcessCmdResult(command, cmdResult, false);
    }
}

QString BridgeApp::file_basename(const QString &path) const
{
    int slash = qMax(path.lastIndexOf('/'), path.lastIndexOf('\\'));
    return path.mid(slash + 1);
}

bool BridgeApp::file_exists(const QString &path) const { return QFile::exists(path); }

QString BridgeApp::file_read(QString path) const
{
    if (path.startsWith("~/"))
        path = QDir::home().filePath(path.mid(2));

    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray fileData = file.readAll();
        file.close();
        return QString::fromLatin1(fileData.toBase64());
    } else {
        return "";
    }
}

bool BridgeApp::file_write_text(QString path, const QString &content, bool append) const
{
    if (path.startsWith("~/"))
        path = QDir::home().filePath(path.mid(2));

    QFile file(path);
    QIODevice::OpenMode mode = append ? (QIODevice::WriteOnly | QIODevice::Append) : QIODevice::WriteOnly;
    if (file.open(mode)) {
        QTextStream stream(&file);
        stream << content;
        file.close();
        return true;
    }
    return false;
}

QString BridgeApp::format_size(const int &size) const { return BytesToFormat(size); }

QString BridgeApp::format_time(const QString &format, const int &time) const
{
    QDateTime epochDateTime = QDateTime::fromSecsSinceEpoch(time, QTimeZone("UTC"));
    QDateTime localDateTime = epochDateTime.toTimeZone(QTimeZone::systemTimeZone());
    return localDateTime.toString(format);
}

QJSValue BridgeApp::get_commands(const QString &id) const
{
    QVariantList list;
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if ( mapAgents.contains(id) ) {
        for (auto cmd : mapAgents[id]->commander->GetCommands())
            list.append(cmd);
    }
    return this->scriptEngine->engine()->toScriptValue(list);
}

QString BridgeApp::hash(const QString &algorithm, const int length, const QString &input) { return GenerateHash(algorithm, length, input); }

QJSValue BridgeApp::ids() const
{
    QVariantList list;
    auto mapAgents = scriptEngine->manager()->GetAgents();

    for (auto agent : mapAgents)
        list.append(agent->data.Id);

    return this->scriptEngine->engine()->toScriptValue(list);
}

QJSValue BridgeApp::interfaces() const
{
    QVariantList list;
    auto interfaces = scriptEngine->manager()->GetInterfaces();

    for (auto addr : interfaces)
        list.append(addr);

    return this->scriptEngine->engine()->toScriptValue(list);
}

bool BridgeApp::is64(const QString &id) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id))
        return false;

    return mapAgents[id]->data.Arch == "x64";
}

bool BridgeApp::isactive(const QString &id) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id))
        return false;

    return mapAgents[id]->active;
}

bool BridgeApp::isadmin(const QString &id) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id))
        return false;

    return mapAgents[id]->data.Elevated;
}

void BridgeApp::log(const QString &text) { Q_EMIT consoleMessage(text); }

void BridgeApp::log_error(const QString &text) { Q_EMIT consoleError(text); }

void BridgeApp::open_agent_console(const QString &id) { scriptEngine->manager()->GetAdaptix()->LoadConsoleUI(id); }

void BridgeApp::open_access_tunnel(const QString &id, const bool socks4, const bool socks5, const bool lportfwd, const bool rportfwd) { scriptEngine->manager()->GetAdaptix()->ShowTunnelCreator(id, socks4, socks5, lportfwd, rportfwd); }

void BridgeApp::open_browser_files(const QString &id) { scriptEngine->manager()->GetAdaptix()->LoadFileBrowserUI(id); }

void BridgeApp::open_browser_process(const QString &id) { scriptEngine->manager()->GetAdaptix()->LoadProcessBrowserUI(id); }

void BridgeApp::open_remote_terminal(const QString &id) { scriptEngine->manager()->GetAdaptix()->LoadTerminalUI(id); }

bool BridgeApp::prompt_confirm(const QString &title, const QString &text)
{
    QMessageBox::StandardButton reply = QMessageBox::question(nullptr, title, text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return (reply == QMessageBox::Yes);
}

QString BridgeApp::prompt_open_file(const QString &caption, const QString &filter)
{
    auto adaptix = scriptEngine->manager()->GetAdaptix();
    QString baseDir = QDir::homePath();
    if (adaptix && adaptix->GetProfile())
        baseDir = adaptix->GetProfile()->GetProjectDir();

    return QFileDialog::getOpenFileName(nullptr, caption, baseDir, filter);
}

QString BridgeApp::prompt_open_dir(const QString &caption)
{
    auto adaptix = scriptEngine->manager()->GetAdaptix();
    QString baseDir = QDir::homePath();
    if (adaptix && adaptix->GetProfile())
        baseDir = adaptix->GetProfile()->GetProjectDir();

    return QFileDialog::getExistingDirectory(nullptr, caption, baseDir);
}

QString BridgeApp::prompt_save_file(const QString &filename, const QString &caption, const QString &filter)
{
    auto adaptix = scriptEngine->manager()->GetAdaptix();
    QString baseDir = QDir::homePath();
    if (adaptix && adaptix->GetProfile())
        baseDir = adaptix->GetProfile()->GetProjectDir();

    QString initialPath = filename;
    if (!QDir::isAbsolutePath(initialPath))
        initialPath = QDir(baseDir).filePath(initialPath);

    return QFileDialog::getSaveFileName(nullptr, caption, initialPath,  filter);
}

QString BridgeApp::random_string(const int length, const QString &setname) { return GenerateRandomString(length, setname); }

int BridgeApp::random_int(const int min, const int max) { return GenerateRandomInt(min, max); }

void BridgeApp::register_commands_group(QObject *obj, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners)
{
    if (!AxScriptUtils::isValidArray(agents)) {
        Q_EMIT engineError("register_commands_group expected array of strings in agents parameter!");
        return;
    }

    if (!AxScriptUtils::isOptionalValidArray(os)) {
        Q_EMIT engineError("register_commands_group expected array of strings in os parameter!");
        return;
    }

    if (!AxScriptUtils::isOptionalValidArray(listeners)) {
        Q_EMIT engineError("register_commands_group expected array of strings in listeners parameter!");
        return;
    }

    auto wrapper = qobject_cast<AxCommandGroupWrapper*>(obj);
    if (!wrapper) {
        Q_EMIT engineError("register_commands_group no support object type!");
        return;
    }

    CommandsGroup commandsGroup = {};
    commandsGroup.groupName = wrapper->getName();
    commandsGroup.commands  = wrapper->getCommands();
    commandsGroup.engine    = wrapper->getEngine();
    commandsGroup.filepath  = scriptEngine->context.name;

    scriptEngine->manager()->RegisterCommandsGroup(
        commandsGroup,
        AxScriptUtils::jsArrayToStringList(listeners),
        AxScriptUtils::jsArrayToStringList(agents),
        AxScriptUtils::parseOsList(os)
    );
}

void BridgeApp::script_import(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return scriptEngine->engine()->throwError("Could not open script: " + path);
    }
    QTextStream in(&file);
    QString code = in.readAll();
    file.close();

    scriptEngine->engine()->evaluate(code, path);
}

void BridgeApp::script_load(const QString &path) { scriptEngine->manager()->GlobalScriptLoad(path); }

void BridgeApp::script_unload(const QString &path) { scriptEngine->manager()->GlobalScriptUnload(path); }

QString BridgeApp::script_dir() { return GetParentPathUnix(scriptEngine->context.name) + "/"; }

QJSValue BridgeApp::screenshots()
{
    QVariantMap list;
    auto screenshots = scriptEngine->manager()->GetScreenshots();

    for (auto screen : screenshots) {
        QVariantMap map;
        map["id"]       = screen.ScreenId;
        map["user"]     = screen.User;
        map["computer"] = screen.Computer;
        map["note"]     = screen.Note;
        map["date"]     = screen.Date;
        list[screen.ScreenId] = map;
    }

    return this->scriptEngine->engine()->toScriptValue(list);
}

void BridgeApp::show_message(const QString &title, const QString &text) { QMessageBox::information(nullptr, title, text); }

QJSValue BridgeApp::targets() const
{
    QVariantMap list;
    auto targets = scriptEngine->manager()->GetTargets();

    for (auto target : targets) {

        QVariantList sessions;
        for (auto agent : target.Agents)
            sessions << agent;

        QVariantMap map;
        map["id"]       = target.TargetId;
        map["computer"] = target.Computer;
        map["domain"]   = target.Domain;
        map["address"]  = target.Address;
        map["tag"]      = target.Tag;
        map["date"]     = target.Date;
        map["info"]     = target.Info;
        map["alive"]    = target.Alive;
        map["agents"]   = sessions;
        map["os_desc"]  = target.OsDesc;

        if (target.Os == OS_WINDOWS)    map["os"] = "windows";
        else if (target.Os == OS_LINUX) map["os"] = "linux";
        else if (target.Os == OS_MAC)   map["os"] = "macos";
        else                            map["os"] = "unknown";

        list[target.TargetId] = map;
    }

    return this->scriptEngine->engine()->toScriptValue(list);
}

void BridgeApp::targets_add(const QString &computer, const QString &domain, const QString &address, const QString &os, const QString &osDesc, const QString &tag, const QString &info, bool alive)
{
    TargetData target = {"", computer, domain, address, tag, QIcon(), 0, osDesc, "", 0, info, alive};

    if (os == "windows")    target.Os = OS_WINDOWS;
    else if (os == "linux") target.Os = OS_LINUX;
    else if (os == "macos") target.Os = OS_MAC;
    else                    target.Os = OS_UNKNOWN;

    QList<TargetData> targets;
    targets.append(target);
    scriptEngine->manager()->GetAdaptix()->TargetsDock->TargetsAdd(targets);
}

void BridgeApp::targets_add_list(const QVariantList &array)
{
    QList<TargetData> targets;

    for (const QVariant &item : array) {
        QVariantMap map = item.toMap();
        TargetData td = {};
        if (map.contains("computer")) td.Computer = map["computer"].toString();
        if (map.contains("domain")) td.Domain = map["domain"].toString();
        if (map.contains("address")) td.Address = map["address"].toString();
        if (map.contains("tag")) td.Tag = map["tag"].toString();
        if (map.contains("info")) td.Info = map["info"].toString();
        if (map.contains("alive")) td.Alive = map["alive"].toBool();
        if (map.contains("os_desc")) td.OsDesc = map["os_desc"].toString();
        if (map.contains("os")) {
            QString os = map["os"].toString();
            if (os == "windows")    td.Os = OS_WINDOWS;
            else if (os == "linux") td.Os = OS_LINUX;
            else if (os == "macos") td.Os = OS_MAC;
            else                    td.Os = OS_UNKNOWN;
        }
        targets.append(td);
    }

    if (targets.isEmpty())
        return;

    scriptEngine->manager()->GetAdaptix()->TargetsDock->TargetsAdd(targets);
}

int BridgeApp::ticks() { return QDateTime::currentSecsSinceEpoch(); }

QJSValue BridgeApp::tunnels()
{
    QVariantMap list;
    auto tunnels = scriptEngine->manager()->GetTunnels();

    for (auto tun : tunnels) {
        QVariantMap map;
        map["id"]        = tun.TunnelId;
        map["agent_id"]  = tun.AgentId;
        map["username"]  = tun.Username;
        map["computer"]  = tun.Computer;
        map["process"]   = tun.Process;
        map["type"]      = tun.Type;
        map["info"]      = tun.Info;
        map["interface"] = tun.Interface;
        map["port"]      = tun.Port;
        map["client"]    = tun.Client;
        map["f_port"]    = tun.Fport;
        map["f_host"]    = tun.Fhost;
        list[tun.TunnelId]  = map;
    }

    return this->scriptEngine->engine()->toScriptValue(list);
}

QJSValue BridgeApp::validate_command(const QString &id, const QString &command) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    QVariantMap result;

    if (!mapAgents.contains(id)) {
        result["valid"] = false;
        result["message"] = "Agent not found";
        return scriptEngine->engine()->toScriptValue(result);
    }

    auto cmdResult = mapAgents[id]->commander->ProcessInput(id, command);
    result["valid"]         = !cmdResult.error;
    result["message"]       = cmdResult.message;
    result["is_pre_hook"]   = cmdResult.is_pre_hook;
    result["has_output"]    = cmdResult.output;
    result["has_post_hook"] = cmdResult.post_hook.isSet;
    if (!cmdResult.error)
        result["parsed"] = cmdResult.data.toVariantMap();

    return scriptEngine->engine()->toScriptValue(result);
}
