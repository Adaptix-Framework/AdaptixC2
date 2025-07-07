#include <main.h>
#include <Agent/Agent.h>
#include <Client/AxScript/BridgeApp.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/AxCommandWrappers.h>
#include <Client/AxScript/AxScriptManager.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/AdaptixWidget.h>

BridgeApp::BridgeApp(AxScriptEngine* scriptEngine, QObject* parent) : QObject(parent), scriptEngine(scriptEngine), widget(new QWidget()){}

BridgeApp::~BridgeApp() { delete widget; }

AxScriptEngine* BridgeApp::GetScriptEngine() const { return this->scriptEngine; }



QJSValue BridgeApp::agents() const
{
    QVariantList list;
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

        list << map;
    }

    return this->scriptEngine->engine()->toScriptValue(list);
}

QString BridgeApp::bof_pack(const QString &types, const QJSValue &args) const
{
    if (!args.isArray()) {
        scriptEngine->engine()->throwError(QJSValue::TypeError, "bof_pack expected array of arguments!");
        return "";
    }

    QStringList items = types.split(",", Qt::SkipEmptyParts);
    int length = args.property("length").toInt();

    if (items.size() != length) {
        scriptEngine->engine()->throwError(QJSValue::TypeError, "bof_pack expects the same number of types and arguments!");
        return "";
    }

    QByteArray data;

    for (int i = 0; i < length; ++i) {
        QVariant value = args.property(i).toVariant();

        if (items[i] == "cstr") {
            if (!value.canConvert<QString>()) {
                scriptEngine->engine()->throwError(QJSValue::TypeError, QString("bof_pack cannot convert argument at index %1 to string").arg(i));
                return "";
            }

            QByteArray valueData = value.toString().toUtf8();

            int strLength = valueData.isEmpty() ? 0 : valueData.size() + 1;

            QByteArray valueLengthData;
            valueLengthData.append(reinterpret_cast<const char*>(&strLength), 4);
            data.append(valueLengthData);

            if (!valueData.isEmpty()) {
                valueData.append('\0');
                data.append(valueData);
            }
        }
        else if (items[i] == "wstr") {
            if (!value.canConvert<QString>()) {
                scriptEngine->engine()->throwError(QJSValue::TypeError, QString("bof_pack cannot convert argument at index %1 to string").arg(i));
                return "";
            }

            QString str = value.toString();

            if (str.size() == 0) {
                QByteArray valueLengthData;
                int strLength = 0;
                valueLengthData.append(reinterpret_cast<const char*>(&strLength), 4);

                data.append(valueLengthData);
            }
            else {
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
        }
        else if (items[i] == "bytes") {
            if (!value.canConvert<QByteArray>()) {
                scriptEngine->engine()->throwError(QJSValue::TypeError, QString("bof_pack cannot convert argument at index %1 to bytes").arg(i));
                return "";
            }

            QByteArray bytes = value.toByteArray();

            QByteArray bytesLengthData;
            int bytesLength = bytes.size();
            bytesLengthData.append(reinterpret_cast<const char*>(&bytesLength), 4);

            data.append(bytesLengthData);
            if (bytesLength > 0)
                data.append(bytes);
        }
        else if (items[i] == "int") {
            if (!value.canConvert<int>()) {
                scriptEngine->engine()->throwError(QJSValue::TypeError, QString("bof_pack cannot convert argument at index %1 to int").arg(i));
                return "";
            }

            int num = value.toInt();
            QByteArray numData;
            numData.append(reinterpret_cast<const char*>(&num), sizeof(num));
            data.append(numData);
        }
        else if (items[i] == "short") {
            if (!value.canConvert<int>()) {
                scriptEngine->engine()->throwError(QJSValue::TypeError, QString("bof_pack cannot convert argument at index %1 to short").arg(i));
                return "";
            }

            short num  = static_cast<short>(value.toInt());
            QByteArray numData;
            numData.append(reinterpret_cast<const char*>(&num), sizeof(num));
            data.append(numData);
        }
        else {
            scriptEngine->engine()->throwError(QJSValue::TypeError, QString("bof_pack does not expect type '%1' (index %2)").arg(items[i]).arg(i));
            return "";
        }
    }

    QByteArray strLengthData;
    int strLength = data.size();
    strLengthData.append(reinterpret_cast<const char*>(&strLength), sizeof(strLength));

    strLengthData.append(data);
    return strLengthData.toBase64();
}

QObject* BridgeApp::create_command(const QString &name, const QString &description, const QString &example, const QString &message)
{
    auto* wrapper = new AxCommandWrappers(name, description, example, message, this);
    connect(wrapper, &AxCommandWrappers::consoleAppendError, this, &BridgeApp::consoleAppendError);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeApp::create_commands_group(const QString &name, const QJSValue &array)
{
    auto* wrapper = new AxCommandGroupWrapper(name, array, scriptEngine->engine(), this);
    connect(wrapper, &AxCommandGroupWrapper::consoleAppendError, this, &BridgeApp::consoleAppendError);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

void BridgeApp::execute_alias(const QString &id, const QString &cmdline, const QString &command) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id))
        return;

    auto agent = mapAgents[id];
    if (!agent)
        return;

    auto cmdResult = agent->commander->ProcessInput(id, command);
    if (!cmdResult.hooked)
        agent->Console->ProcessCmdResult(cmdline, cmdResult);
}

void BridgeApp::execute_command(const QString &id, const QString &command) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id))
        return;

    auto agent = mapAgents[id];
    if (!agent)
        return;

    auto cmdResult = agent->commander->ProcessInput(id, command);
    if (!cmdResult.hooked)
        agent->Console->ProcessCmdResult(command, cmdResult);

}

bool BridgeApp::is64(const QString &id) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id))
        return false;

    return mapAgents[id]->data.Arch == "x64";
}

void BridgeApp::log(const QString &text) { emit consoleAppend(text); }

void BridgeApp::log_error(const QString &text) { emit consoleAppendError(text); }

void BridgeApp::open_access_tunnel(const QString &id, const bool socks4, const bool socks5, const bool lportfwd, const bool rportfwd) { scriptEngine->manager()->GetAdaptix()->ShowTunnelCreator(id, socks4, socks5, lportfwd, rportfwd); }

void BridgeApp::open_browser_files(const QString &id) { scriptEngine->manager()->GetAdaptix()->LoadFileBrowserUI(id); }

void BridgeApp::open_browser_process(const QString &id) { scriptEngine->manager()->GetAdaptix()->LoadProcessBrowserUI(id); }

void BridgeApp::open_browser_terminal(const QString &id) { scriptEngine->manager()->GetAdaptix()->LoadTerminalUI(id); }

void BridgeApp::register_commands_group(QObject *obj, const QJSValue &os, const QJSValue &agents, const QJSValue &listeners) const
{
    QStringList list_os;
    QStringList list_agents;
    QStringList list_listeners;

    if (os.isUndefined() || os.isNull() || !os.isArray()) {
        scriptEngine->engine()->throwError(QJSValue::TypeError, "register_commands_group expected array of strings in os parameter!");;
        return;
    }

    if (agents.isUndefined() || agents.isNull() || !agents.isArray()) {
        scriptEngine->engine()->throwError(QJSValue::TypeError, "register_commands_group expected array of strings in agents parameter!");;
        return;
    }

    if (listeners.isUndefined() || listeners.isNull() || !listeners.isArray()) {
        scriptEngine->engine()->throwError(QJSValue::TypeError, "register_commands_group expected array of strings in listeners parameter!");;
        return;
    }

    for (int i = 0; i < os.property("length").toInt(); ++i) {
        QJSValue val = os.property(i);
        list_os << val.toString();
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
    if (wrapper) {
        CommandsGroup commandsGroup;
        commandsGroup.groupName = wrapper->getName();
        commandsGroup.commands  = wrapper->getCommands();
        commandsGroup.engine    = wrapper->getEngine();

        /// TODO: register_commands_group

        // main->commander->AddAxCommands(commandsGroup);
    } else {
        scriptEngine->engine()->throwError(QJSValue::TypeError, "register_commands_group no support object type!");;
    }
}
