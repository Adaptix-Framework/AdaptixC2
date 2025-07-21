#include <main.h>
#include <QTimeZone>
#include <Agent/Agent.h>
#include <Client/AxScript/BridgeApp.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/AxCommandWrappers.h>
#include <Client/AxScript/AxScriptManager.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/CredentialsWidget.h>

BridgeApp::BridgeApp(AxScriptEngine* scriptEngine, QObject* parent) : QObject(parent), scriptEngine(scriptEngine), widget(new QWidget()){}

BridgeApp::~BridgeApp() { delete widget; }

AxScriptEngine* BridgeApp::GetScriptEngine() const { return this->scriptEngine; }



QJSValue BridgeApp::agents() const
{
    QVariantMap list;
    auto mapAgents = scriptEngine->manager()->GetAgents();

    for (auto agent : mapAgents) {
        QVariantMap map;
        map["id"]          = agent->data.Id;
        map["type"   ]     = agent->data.Name;
        map["listener"]    = agent->data.Listener;
        map["external_ip"] = agent->data.ExternalIP;
        map["internal_ip"] = agent->data.InternalIP;
        map["domain"]      = agent->data.Domain;
        map["computer"]    = agent->data.Computer;
        map["username"]    = agent->data.Username;
        map["process"]     = agent->data.Process;
        map["arch"]        = agent->data.Arch;
        map["pid"]         = agent->data.Pid.toInt();
        map["tid"]         = agent->data.Tid.toInt();
        map["gmt"]         = agent->data.GmtOffset;
        map["elevated"]    = agent->data.Elevated;
        map["tags"]        = agent->data.Tags;
        map["async"]       = agent->data.Async;
        map["sleep"]       = agent->data.Sleep;
        map["os_full"]     = agent->data.OsDesc;

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
        emit engineError("bof_pack expected array of arguments!");
        return "";
    }

    QStringList items = types.split(",", Qt::SkipEmptyParts);
    int length = args.property("length").toInt();

    if (items.size() != length) {
        emit engineError("bof_pack expects the same number of types and arguments!");
        return "";
    }

    QByteArray data;

    for (int i = 0; i < length; ++i) {
        QVariant value = args.property(i).toVariant();

        if (items[i] == "cstr") {
            if (!value.canConvert<QString>()) {
                emit engineError(QString("bof_pack cannot convert argument at index %1 to string").arg(i));
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
                emit engineError(QString("bof_pack cannot convert argument at index %1 to string").arg(i));
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
                emit engineError(QString("bof_pack cannot convert argument at index %1 to string").arg(i));
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
                emit engineError(QString("bof_pack cannot convert argument at index %1 to int").arg(i));
                return "";
            }

            int num = value.toInt();
            QByteArray numData;
            numData.append(reinterpret_cast<const char*>(&num), sizeof(num));
            data.append(numData);
        }
        else if (items[i] == "short") {
            if (!value.canConvert<int>()) {
                emit engineError(QString("bof_pack cannot convert argument at index %1 to short").arg(i));
                return "";
            }

            short num  = static_cast<short>(value.toInt());
            QByteArray numData;
            numData.append(reinterpret_cast<const char*>(&num), sizeof(num));
            data.append(numData);
        }
        else {
            emit engineError(QString("bof_pack does not expect type '%1' (index %2)").arg(items[i]).arg(i));
            return "";
        }
    }

    QByteArray strLengthData;
    int strLength = data.size();
    strLengthData.append(reinterpret_cast<const char*>(&strLength), sizeof(strLength));

    strLengthData.append(data);
    return strLengthData.toBase64();
}

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

void BridgeApp::credentials_add(const QString &username, const QString &password, const QString &realm, const QString &type, const QString &tag, const QString &storage, const QString &host) { scriptEngine->manager()->GetAdaptix()->CredentialsTab->CredentialsAdd(username, password, realm, type, tag, storage, host); }

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
        if (!message.isEmpty()) {
            cmdResult.data["message"] = message;
        }

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

QString BridgeApp::format_time(const QString &format, const int &time) const
{
    QDateTime epochDateTime = QDateTime::fromSecsSinceEpoch(time, QTimeZone("UTC"));
    QDateTime localDateTime = epochDateTime.toTimeZone(QTimeZone::systemTimeZone());
    return localDateTime.toString(format);
}

bool BridgeApp::is64(const QString &id) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id))
        return false;

    return mapAgents[id]->data.Arch == "x64";
}

bool BridgeApp::isadmin(const QString &id) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id))
        return false;

    return mapAgents[id]->data.Elevated;
}

void BridgeApp::log(const QString &text) { emit consoleMessage(text); }

void BridgeApp::log_error(const QString &text) { emit consoleError(text); }

void BridgeApp::open_agent_console(const QString &id) { scriptEngine->manager()->GetAdaptix()->LoadConsoleUI(id); }

void BridgeApp::open_access_tunnel(const QString &id, const bool socks4, const bool socks5, const bool lportfwd, const bool rportfwd) { scriptEngine->manager()->GetAdaptix()->ShowTunnelCreator(id, socks4, socks5, lportfwd, rportfwd); }

void BridgeApp::open_browser_files(const QString &id) { scriptEngine->manager()->GetAdaptix()->LoadFileBrowserUI(id); }

void BridgeApp::open_browser_process(const QString &id) { scriptEngine->manager()->GetAdaptix()->LoadProcessBrowserUI(id); }

void BridgeApp::open_remote_terminal(const QString &id) { scriptEngine->manager()->GetAdaptix()->LoadTerminalUI(id); }

void BridgeApp::register_commands_group(QObject *obj, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners)
{
    QList<int> list_os;
    QStringList list_agents;
    QStringList list_listeners;

    if (agents.isUndefined() || agents.isNull() || !agents.isArray()) {
        emit engineError("register_commands_group expected array of strings in agents parameter!");
        return;
    }

    if (os.isUndefined() && (os.isNull() || !os.isArray()) ) {
        emit engineError("register_commands_group expected array of strings in os parameter!");
        return;
    }

    if (listeners.isUndefined() && (listeners.isNull() || !listeners.isArray())) {
        emit engineError("register_commands_group expected array of strings in listeners parameter!");
        return;
    }

    for (int i = 0; i < os.property("length").toInt(); ++i) {
        QJSValue val = os.property(i);
        if (val.toString() == "windows") list_os.append(1);
        else if (val.toString() == "linux") list_os.append(2);
        else if (val.toString() == "macos") list_os.append(3);
    }

    for (int i = 0; i < agents.property("length").toInt(); ++i) {
        QJSValue val = agents.property(i);
        list_agents << val.toString();
    }

    for (int i = 0; i < listeners.property("length").toInt(); ++i) {
        QJSValue val = listeners.property(i);
        list_listeners << val.toString();
    }

    auto wrapper = qobject_cast<AxCommandGroupWrapper*>(obj);
    if (!wrapper) {
        emit engineError("register_commands_group no support object type!");
        return;
    }

    CommandsGroup commandsGroup = {};
    commandsGroup.groupName = wrapper->getName();
    commandsGroup.commands  = wrapper->getCommands();
    commandsGroup.engine    = wrapper->getEngine();
    commandsGroup.filepath  = scriptEngine->context.name;

    scriptEngine->manager()->RegisterCommandsGroup(commandsGroup, list_listeners, list_agents, list_os);
}

void BridgeApp::script_load(const QString &path) { scriptEngine->manager()->GlobalScriptLoad(path); }

void BridgeApp::script_unload(const QString &path) { scriptEngine->manager()->GlobalScriptUnload(path); }

QString BridgeApp::script_dir()
{
#ifdef Q_OS_WIN
    return GetParentPathWindows(scriptEngine->context.name) + "\\";
#else
    return GetParentPathUnix(scriptEngine->context.name) + "/";
#endif
}

void BridgeApp::show_message(const QString &title, const QString &text) { QMessageBox::information(nullptr, title, text); }

int BridgeApp::ticks() { return QDateTime::currentSecsSinceEpoch(); }
