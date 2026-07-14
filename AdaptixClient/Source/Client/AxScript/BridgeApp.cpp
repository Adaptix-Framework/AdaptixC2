#include <main.h>
#include <QTimeZone>
#include <QJSValueIterator>
#include <Agent/Agent.h>
#include <Client/AuthProfile.h>
#include <Client/Requestor.h>
#include <Client/AxScript/BridgeApp.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/AxCommandWrappers.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/AxScriptUtils.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/CredentialWidgetIface.h>
#include <UI/Widgets/TargetWidgetIface.h>

namespace {

QString osToString(int os) {
    switch (os) {
        case OS_WINDOWS: return "windows";
        case OS_LINUX:   return "linux";
        case OS_MAC:     return "macos";
        default:         return "unknown";
    }
}

int stringToOs(const QString &os) {
    if (os == "windows") return OS_WINDOWS;
    if (os == "linux")   return OS_LINUX;
    if (os == "macos")   return OS_MAC;
    return OS_UNKNOWN;
}

} // namespace

BridgeApp::BridgeApp(AxScriptEngine* scriptEngine, QObject* parent) : QObject(parent), scriptEngine(scriptEngine) {}

BridgeApp::~BridgeApp() = default;

AxScriptEngine* BridgeApp::GetScriptEngine() const { return this->scriptEngine; }



QJSValue BridgeApp::agents() const
{
    QVariantMap list;
    auto mapAgents = scriptEngine->manager()->GetAgents();

    for (const auto& agent : mapAgents) {
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
        map["acp"]          = agent->data.ACP;
        map["oemcp"]        = agent->data.OemCP;
        map["elevated"]     = agent->data.Elevated;
        map["tags"]         = agent->data.Tags;
        map["async"]        = agent->data.Async;
        map["sleep"]        = agent->data.Sleep;
        map["os_full"]      = agent->data.OsDesc;
        map["os"]           = osToString(agent->data.Os);

        list[QString::number(agent->data.Id)] = map;
    }

    return this->scriptEngine->engine()->toScriptValue(list);
}

QJSValue BridgeApp::agent_info(const QString &id, const QString &property) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id.toLongLong()))
        return false;

    QJSValue ret;
    auto info = mapAgents[id.toLongLong()]->data;

    if (property == "id")
        return QJSValue(QString::number(info.Id));
    if (property == "type")
        return QJSValue(info.Name);
    if (property == "listener")
        return QJSValue(info.Listener);
    if (property == "external_ip")
        return QJSValue(info.ExternalIP);
    if (property == "internal_ip")
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
    if (property == "acp")
        return QJSValue(info.ACP);
    if (property == "oemcp")
        return QJSValue(info.OemCP);
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
    if (property == "os")
        return QJSValue(osToString(info.Os));

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

void BridgeApp::agent_set_impersonate(const QString &id, const QString &impersonate, const bool elevated)
{
    QJsonObject updateData;
    if (impersonate.isEmpty())
        return;

    if (elevated)
        updateData["impersonated"] = impersonate + " *";
    else
        updateData["impersonated"] = impersonate;

    scriptEngine->manager()->AppAgentUpdateData(id, updateData);
}

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

void BridgeApp::agent_update_data(const QString &id, const QJSValue &data)
{
    if (!data.isObject()) {
        Q_EMIT engineError("agent_update_data expected object in data parameter!");
        return;
    }

    QJsonObject updateData;
    QJSValueIterator it(data);
    while (it.hasNext()) {
        it.next();
        QString key = it.name();
        QJSValue val = it.value();

        if (key == "internal_ip" && val.isString())
            updateData["internal_ip"] = val.toString();
        else if (key == "external_ip" && val.isString())
            updateData["external_ip"] = val.toString();
        else if (key == "gmt_offset" && val.isNumber())
            updateData["gmt_offset"] = val.toInt();
        else if (key == "acp" && val.isNumber())
            updateData["acp"] = val.toInt();
        else if (key == "oemcp" && val.isNumber())
            updateData["oemcp"] = val.toInt();
        else if (key == "pid" && val.isString())
            updateData["pid"] = val.toString();
        else if (key == "tid" && val.isString())
            updateData["tid"] = val.toString();
        else if (key == "arch" && val.isString())
            updateData["arch"] = val.toString();
        else if (key == "elevated" && val.isBool())
            updateData["elevated"] = val.toBool();
        else if (key == "process" && val.isString())
            updateData["process"] = val.toString();
        else if (key == "os" && val.isNumber())
            updateData["os"] = val.toInt();
        else if (key == "os_desc" && val.isString())
            updateData["os_desc"] = val.toString();
        else if (key == "domain" && val.isString())
            updateData["domain"] = val.toString();
        else if (key == "computer" && val.isString())
            updateData["computer"] = val.toString();
        else if (key == "username" && val.isString())
            updateData["username"] = val.toString();
        else if (key == "impersonated" && val.isString())
            updateData["impersonated"] = val.toString();
        else if (key == "tags" && val.isString())
            updateData["tags"] = val.toString();
        else if (key == "mark" && val.isString())
            updateData["mark"] = val.toString();
        else if (key == "color" && val.isString())
            updateData["color"] = val.toString();
    }

    if (updateData.isEmpty()) {
        Q_EMIT engineError("agent_update_data: no valid fields provided!");
        return;
    }

    scriptEngine->manager()->AppAgentUpdateData(id, updateData);
}

QString BridgeApp::arch(const QString &id) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id.toLongLong()))
        return "x86";

    return mapAgents[id.toLongLong()]->data.Arch;
}

static QStringList parseBofTypes(const QString &types)
{
    const QString trimmed = types.trimmed();
    if (trimmed.isEmpty())
        return {};

    if (trimmed.contains(','))
        return trimmed.split(',', Qt::SkipEmptyParts);

    if (trimmed == "cstr" || trimmed == "wstr" || trimmed == "bytes" || trimmed == "int"  || trimmed == "short")
        return { trimmed };

    QStringList items;
    for (const QChar ch : trimmed) {
        switch (ch.unicode()) {
            case 'z': items.append("cstr");  break;
            case 'Z': items.append("wstr");  break;
            case 'b': items.append("bytes"); break;
            case 'i': items.append("int");   break;
            case 's': items.append("short"); break;
            default:  items.append(QString(ch)); break;
        }
    }
    return items;
}

QString BridgeApp::bof_pack(const QString &types, const QJSValue &args)
{
    if (!args.isArray()) {
        Q_EMIT engineError("bof_pack expected array of arguments!");
        return "";
    }

    QStringList items = parseBofTypes(types);
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
            // Accept ArrayBuffer / Uint8Array (QByteArray after Qt marshal)
            const QByteArray valueData = value.toByteArray();
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
    if (!mapAgents.contains(id.toLongLong()))
        return;

    auto agent = mapAgents[id.toLongLong()];
    if (!agent || !agent->Console)
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

    for (const auto& cred : vecCreds) {
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

        list[QString::number(cred.CredId)] = map;
    }

    return this->scriptEngine->engine()->toScriptValue(list);
}

void BridgeApp::credentials_add(const QString &username, const QString &password, const QString &realm, const QString &type, const QString &tag, const QString &storage, const QString &host)
{
    CredentialData cred = {0, username, password, realm, type, tag, "", 0, storage, 0, host};

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

    for (const auto& download : mapDownloads) {
        QVariantMap map;
        map["id"]         = QVariant::fromValue(download.FileId);
        map["agent_id"]   = download.AgentId;
        map["agent_name"] = download.AgentName;
        map["user"]       = download.User;
        map["computer"]   = download.Computer;
        map["filename"]   = download.Filename;
        map["recv_size"]  = download.Progress;
        map["total_size"] = download.TotalSize;
        map["date"]       = download.Date;

        switch (download.State) {
            case TRANSFER_STATE_RUNNING:  map["state"] = "running";  break;
            case TRANSFER_STATE_STOPPED:  map["state"] = "stopped";  break;
            case TRANSFER_STATE_FINISHED: map["state"] = "finished"; break;
            default:                      map["state"] = "canceled"; break;
        }

        list[QString::number(download.FileId)] = map;
    }

    return this->scriptEngine->engine()->toScriptValue(list);
}

void BridgeApp::execute_alias(const QString &id, const QString &cmdline, const QString &command, const QString &message, const QJSValue &hook, const QJSValue &handler) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id.toLongLong()))
        return;

    auto agent = mapAgents[id.toLongLong()];
    if (!agent || !agent->Console)
        return;

    auto cmdResult = agent->commander->ProcessInput(id.toLongLong(), command);
    if (!cmdResult.is_pre_hook) {
        if (!message.isEmpty())
            cmdResult.data["message"] = message;

        if (!hook.isUndefined() && !hook.isNull() && hook.isCallable())
            cmdResult.post_hook = {true, scriptEngine->context.name, hook};

        if (!handler.isUndefined() && !handler.isNull() && handler.isCallable())
            cmdResult.handler = {true, scriptEngine->context.name, handler};

        agent->Console->ProcessCmdResult(cmdline, cmdResult, false);
    }
}

void BridgeApp::execute_alias_hook(const QString &id, const QString &cmdline, const QString &command, const QString &message, const QJSValue &hook) const {
    execute_alias(id, cmdline, command, message, hook, QJSValue());
}

void BridgeApp::execute_alias_handler(const QString &id, const QString &cmdline, const QString &command, const QString &message, const QJSValue &handler) const {
    execute_alias(id, cmdline, command, message,  QJSValue(), handler);
}

void BridgeApp::execute_browser(const QString &id, const QString &command) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id.toLongLong()))
        return;

    auto agent = mapAgents[id.toLongLong()];
    if (!agent || !agent->Console)
        return;

    auto cmdResult = agent->commander->ProcessInput(id.toLongLong(), command);
    agent->Console->ProcessCmdResult(command, cmdResult, true);
}

void BridgeApp::execute_command(const QString &id, const QString &command, const QJSValue &hook, const QJSValue &handler) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id.toLongLong()))
        return;

    auto agent = mapAgents[id.toLongLong()];
    if (!agent || !agent->Console)
        return;

    auto cmdResult = agent->commander->ProcessInput(id.toLongLong(), command);
    if (!cmdResult.is_pre_hook) {

        if (!hook.isUndefined() && !hook.isNull() && hook.isCallable())
            cmdResult.post_hook = {true, scriptEngine->context.name, hook};

        if (!handler.isUndefined() && !handler.isNull() && handler.isCallable())
            cmdResult.handler = {true, scriptEngine->context.name, handler};

        agent->Console->ProcessCmdResult(command, cmdResult, false);
    }
}

void BridgeApp::execute_command_hook(const QString &id, const QString &command, const QJSValue &hook) const {
    execute_command(id, command, hook, QJSValue());
}

void BridgeApp::execute_command_handler(const QString &id, const QString &command, const QJSValue &handler) const {
    execute_command(id, command, QJSValue(), handler);
}

QString BridgeApp::file_basename(const QString &path) const
{
    int slash = qMax(path.lastIndexOf('/'), path.lastIndexOf('\\'));
    return path.mid(slash + 1);
}

QString BridgeApp::file_dirname(const QString &path) const
{
    QFileInfo fi(path);
    return fi.absolutePath();
}

QString BridgeApp::file_extension(const QString &path) const
{
    QFileInfo fi(path);
    return fi.suffix();
}

bool BridgeApp::file_exists(const QString &path) const { return QFile::exists(path); }

QByteArray BridgeApp::file_read(QString path) const
{
    if (path.startsWith("~/"))
        path = QDir::home().filePath(path.mid(2));

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QByteArray data = file.readAll();
    file.close();
    return data;
}

qint64 BridgeApp::file_size(const QString &path) const
{
    QFileInfo fi(path);
    return fi.size();
}

bool BridgeApp::file_write(QString path, const QByteArray &data, bool append) const
{
    if (path.startsWith("~/"))
        path = QDir::home().filePath(path.mid(2));

    QFile file(path);
    QIODevice::OpenMode mode = append ? (QIODevice::WriteOnly | QIODevice::Append)
                                      : (QIODevice::WriteOnly | QIODevice::Truncate);
    if (!file.open(mode))
        return false;
    const qint64 written = file.write(data);
    file.close();
    return written == data.size();
}

// Encoding methods

static QByteArray applyXor(const QByteArray &data, const QByteArray &key)
{
    if (key.isEmpty())
        return data;
    QByteArray result;
    result.reserve(data.size());
    for (int i = 0; i < data.size(); ++i)
        result.append(data[i] ^ key[i % key.size()]);
    return result;
}

static QByteArray encodeBase32(const QByteArray &data)
{
    static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    QByteArray result;
    int buffer = 0, bitsLeft = 0;
    for (int i = 0; i < data.size(); ++i) {
        buffer = (buffer << 8) | static_cast<unsigned char>(data[i]);
        bitsLeft += 8;
        while (bitsLeft >= 5) {
            result.append(alphabet[(buffer >> (bitsLeft - 5)) & 0x1F]);
            bitsLeft -= 5;
        }
    }
    if (bitsLeft > 0)
        result.append(alphabet[(buffer << (5 - bitsLeft)) & 0x1F]);
    while (result.size() % 8 != 0)
        result.append('=');
    return result;
}

static QByteArray decodeBase32(const QByteArray &data)
{
    static constexpr int lookup[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,26,27,28,29,30,31,-1,-1,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    QByteArray result;
    int buffer = 0, bitsLeft = 0;
    for (int i = 0; i < data.size(); ++i) {
        if (data[i] == '=') break;
        int val = lookup[static_cast<unsigned char>(data[i])];
        if (val < 0) continue;
        buffer = (buffer << 5) | val;
        bitsLeft += 5;
        if (bitsLeft >= 8) {
            result.append(static_cast<char>((buffer >> (bitsLeft - 8)) & 0xFF));
            bitsLeft -= 8;
        }
    }
    return result;
}

QVariant BridgeApp::encode_data(const QString &algorithm, const QByteArray &data, const QString &key) const
{
    QString alg = algorithm.toLower();

    if (alg == "hex")
        return QVariant(QString::fromLatin1(data.toHex()));
    if (alg == "base64")
        return QVariant(QString::fromLatin1(data.toBase64()));
    if (alg == "base32")
        return QVariant(QString::fromLatin1(encodeBase32(data)));
    if (alg == "zip")
        return QVariant(qCompress(data));
    if (alg == "xor")
        return QVariant(applyXor(data, key.toUtf8()));

    return QVariant(QString::fromLatin1(data.toBase64()));
}

QByteArray BridgeApp::decode_data(const QString &algorithm, const QByteArray &data, const QString &key) const
{
    QString alg = algorithm.toLower();

    if (alg == "hex")
        return QByteArray::fromHex(data);
    if (alg == "base64")
        return QByteArray::fromBase64(data);
    if (alg == "base32")
        return decodeBase32(data);
    if (alg == "zip")
        return qUncompress(data);
    if (alg == "xor")
        return applyXor(data, key.toUtf8());

    return data;
}

QVariant BridgeApp::encode_file(const QString &algorithm, const QString &path, const QString &key) const
{
    QString filePath = path;
    if (filePath.startsWith("~/"))
        filePath = QDir::home().filePath(filePath.mid(2));

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return QVariant(QString(""));

    QByteArray bytes = file.readAll();
    file.close();

    return encode_data(algorithm, bytes, key);
}

QByteArray BridgeApp::decode_file(const QString &algorithm, const QString &path, const QString &key) const
{
    QString filePath = path;
    if (filePath.startsWith("~/"))
        filePath = QDir::home().filePath(filePath.mid(2));

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();

    QByteArray rawData = file.readAll();
    file.close();

    QString alg = algorithm.toLower();

    if (alg == "hex")
        return QByteArray::fromHex(rawData.trimmed());
    if (alg == "base64")
        return QByteArray::fromBase64(rawData);
    if (alg == "base32")
        return decodeBase32(rawData);
    if (alg == "zip")
        return qUncompress(rawData);
    if (alg == "xor")
        return applyXor(rawData, key.toUtf8());

    return rawData;
}

// Code conversion

static QString bytesToCode_C(const QByteArray &data, const QString &varName)
{
    QString result = QString("unsigned char %1[%2] = {\n    ").arg(varName).arg(data.size());
    for (int i = 0; i < data.size(); ++i) {
        result += QString("0x%1").arg(static_cast<unsigned char>(data[i]), 2, 16, QChar('0'));
        if (i < data.size() - 1) {
            result += ", ";
            if ((i + 1) % 12 == 0) result += "\n    ";
        }
    }
    return result + "\n};";
}

static QString bytesToCode_CSharp(const QByteArray &data, const QString &varName)
{
    QString result = QString("byte[] %1 = new byte[%2] {\n    ").arg(varName).arg(data.size());
    for (int i = 0; i < data.size(); ++i) {
        result += QString("0x%1").arg(static_cast<unsigned char>(data[i]), 2, 16, QChar('0'));
        if (i < data.size() - 1) {
            result += ", ";
            if ((i + 1) % 12 == 0) result += "\n    ";
        }
    }
    return result + "\n};";
}

static QString bytesToCode_Python(const QByteArray &data, const QString &varName)
{
    QString result = QString("%1 = b\"").arg(varName);
    for (int i = 0; i < data.size(); ++i)
        result += QString("\\x%1").arg(static_cast<unsigned char>(data[i]), 2, 16, QChar('0'));
    return result + "\"";
}

static QString bytesToCode_Golang(const QByteArray &data, const QString &varName)
{
    QString result = QString("%1 := []byte{\n    ").arg(varName);
    for (int i = 0; i < data.size(); ++i) {
        result += QString("0x%1").arg(static_cast<unsigned char>(data[i]), 2, 16, QChar('0'));
        if (i < data.size() - 1) {
            result += ", ";
            if ((i + 1) % 12 == 0) result += "\n    ";
        }
    }
    return result + "\n}";
}

static QString bytesToCode_VBS(const QByteArray &data, const QString &varName)
{
    QString result = QString("%1 = Array(").arg(varName);
    for (int i = 0; i < data.size(); ++i) {
        result += QString("&H%1").arg(static_cast<unsigned char>(data[i]), 2, 16, QChar('0')).toUpper();
        if (i < data.size() - 1) {
            result += ", ";
            if ((i + 1) % 10 == 0) result += " _\n    ";
        }
    }
    return result + ")";
}

static QString bytesToCode_Nim(const QByteArray &data, const QString &varName)
{
    QString result = QString("var %1: array[%2, byte] = [\n    byte ").arg(varName).arg(data.size());
    for (int i = 0; i < data.size(); ++i) {
        result += QString("0x%1").arg(static_cast<unsigned char>(data[i]), 2, 16, QChar('0'));
        if (i < data.size() - 1) {
            result += ", ";
            if ((i + 1) % 12 == 0) result += "\n    ";
        }
    }
    return result + "\n]";
}

static QString bytesToCode_Rust(const QByteArray &data, const QString &varName)
{
    QString result = QString("let %1: [u8; %2] = [\n    ").arg(varName).arg(data.size());
    for (int i = 0; i < data.size(); ++i) {
        result += QString("0x%1").arg(static_cast<unsigned char>(data[i]), 2, 16, QChar('0'));
        if (i < data.size() - 1) {
            result += ", ";
            if ((i + 1) % 12 == 0) result += "\n    ";
        }
    }
    return result + "\n];";
}

static QString bytesToCode_PowerShell(const QByteArray &data, const QString &varName)
{
    QString result = QString("[Byte[]] $%1 = @(\n    ").arg(varName);
    for (int i = 0; i < data.size(); ++i) {
        result += QString("0x%1").arg(static_cast<unsigned char>(data[i]), 2, 16, QChar('0'));
        if (i < data.size() - 1) {
            result += ", ";
            if ((i + 1) % 12 == 0) result += "\n    ";
        }
    }
    return result + "\n)";
}

QString BridgeApp::convert_to_code(const QString &language, const QByteArray &data, const QString &varName) const
{
    QString lang = language.toLower();

    if (lang == "c" || lang == "cpp" || lang == "c++")
        return bytesToCode_C(data, varName);
    if (lang == "csharp" || lang == "cs" || lang == "c#")
        return bytesToCode_CSharp(data, varName);
    if (lang == "python" || lang == "py")
        return bytesToCode_Python(data, varName);
    if (lang == "golang" || lang == "go")
        return bytesToCode_Golang(data, varName);
    if (lang == "vbs" || lang == "vbscript")
        return bytesToCode_VBS(data, varName);
    if (lang == "nim")
        return bytesToCode_Nim(data, varName);
    if (lang == "rust" || lang == "rs")
        return bytesToCode_Rust(data, varName);
    if (lang == "powershell" || lang == "ps" || lang == "ps1")
        return bytesToCode_PowerShell(data, varName);

    return "";
}

QString BridgeApp::format_size(const qint64 &size) const { return BytesToFormat(size); }

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
    if ( mapAgents.contains(id.toLongLong()) ) {
        for (auto cmd : mapAgents[id.toLongLong()]->commander->GetCommands())
            list.append(cmd);
    }
    return this->scriptEngine->engine()->toScriptValue(list);
}

QString BridgeApp::hash(const QString &algorithm, const int length, const QByteArray &input) { return GenerateHash(algorithm, length, input); }

QJSValue BridgeApp::ids() const
{
    QVariantList list;
    auto mapAgents = scriptEngine->manager()->GetAgents();

    for (const auto& agent : mapAgents)
        list.append(QString::number(agent->data.Id));

    return this->scriptEngine->engine()->toScriptValue(list);
}

QJSValue BridgeApp::interfaces() const
{
    QVariantList list;
    auto interfaces = scriptEngine->manager()->GetInterfaces();

    for (const auto& addr : interfaces)
        list.append(addr);

    return this->scriptEngine->engine()->toScriptValue(list);
}

bool BridgeApp::is64(const QString &id) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id.toLongLong()))
        return false;

    return mapAgents[id.toLongLong()]->data.Arch == "x64";
}

bool BridgeApp::isactive(const QString &id) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id.toLongLong()))
        return false;

    return mapAgents[id.toLongLong()]->active;
}

bool BridgeApp::isadmin(const QString &id) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    if (!mapAgents.contains(id.toLongLong()))
        return false;

    return mapAgents[id.toLongLong()]->data.Elevated;
}

void BridgeApp::log(const QString &text) { Q_EMIT consoleMessage(text); }

void BridgeApp::log_error(const QString &text) { Q_EMIT consoleError(text); }

void BridgeApp::open_agent_console(const QString &id) { scriptEngine->manager()->GetAdaptix()->LoadConsoleUI(id.toLongLong()); }

void BridgeApp::open_access_tunnel(const QString &id, const bool socks4, const bool socks5, const bool lportfwd, const bool rportfwd) { scriptEngine->manager()->GetAdaptix()->ShowTunnelCreator(id.toLongLong(), socks4, socks5, lportfwd, rportfwd); }

void BridgeApp::open_browser_files(const QString &id) { scriptEngine->manager()->GetAdaptix()->LoadFileBrowserUI(id.toLongLong()); }

void BridgeApp::open_browser_process(const QString &id) { scriptEngine->manager()->GetAdaptix()->LoadProcessBrowserUI(id.toLongLong()); }

void BridgeApp::open_remote_terminal(const QString &id) { scriptEngine->manager()->GetAdaptix()->LoadTerminalUI(id.toLongLong()); }

void BridgeApp::open_remote_shell(const QString &id) { scriptEngine->manager()->GetAdaptix()->LoadShellUI(id.toLongLong()); }

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
    if (scriptEngine->isServerMode())
        return;

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
    if (scriptEngine->isServerMode()) {
        return; //scriptEngine->engine()->throwError(QStringLiteral("script_import is not available for server scripts"));
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return scriptEngine->engine()->throwError("Could not open script: " + path);
    }
    QTextStream in(&file);
    QString code = in.readAll();
    file.close();

    scriptEngine->engine()->evaluate(code, path);
}

void BridgeApp::script_load(const QString &path)
{
    if (scriptEngine->isServerMode()) {
        return; //scriptEngine->engine()->throwError(QStringLiteral("script_load is not available for server scripts"));
    }
    scriptEngine->manager()->GlobalScriptLoad(path);
}

void BridgeApp::script_unload(const QString &path)
{
    if (scriptEngine->isServerMode()) {
        return; // scriptEngine->engine()->throwError(QStringLiteral("script_unload is not available for server scripts"));
    }
    scriptEngine->manager()->GlobalScriptUnload(path);
}

QString BridgeApp::script_dir() { return GetParentPathUnix(scriptEngine->context.name) + "/"; }

QString BridgeApp::get_project() const
{
    auto adaptix = scriptEngine->manager()->GetAdaptix();
    if (adaptix && adaptix->GetProfile())
        return adaptix->GetProfile()->GetProject();
    return QString();
}

QJSValue BridgeApp::screenshots()
{
    QVariantMap list;
    auto screenshots = scriptEngine->manager()->GetScreenshots();

    for (const auto& screen : screenshots) {
        QVariantMap map;
        map["id"]       = screen.ScreenId;
        map["user"]     = screen.User;
        map["computer"] = screen.Computer;
        map["note"]     = screen.Note;
        map["date"]     = screen.Date;
        list[QString::number(screen.ScreenId)] = map;    }

    return this->scriptEngine->engine()->toScriptValue(list);
}

void BridgeApp::service_command(const QString &service, const QString &command, const QJSValue &args)
{
    QString argsStr;
    if (!args.isUndefined() && !args.isNull()) {
        if (!args.isObject()) {
            Q_EMIT engineError("service_command expected object in args parameter!");
            return;
        }
        QJsonObject argsObj = QJsonObject::fromVariantMap(args.toVariant().toMap());
        argsStr = QString::fromUtf8(QJsonDocument(argsObj).toJson(QJsonDocument::Compact));
    }

    auto adaptix = scriptEngine->manager()->GetAdaptix();
    if (!adaptix || !adaptix->GetProfile()) {
        Q_EMIT engineError("service_command: no active profile!");
        return;
    }

    HttpReqServiceCallAsync(service, command, argsStr, *adaptix->GetProfile(), [](bool, const QString&, const QJsonObject&) {});
}

void BridgeApp::show_message(const QString &title, const QString &text) { QMessageBox::information(nullptr, title, text); }

QJSValue BridgeApp::targets() const
{
    QVariantMap list;
    auto targets = scriptEngine->manager()->GetTargets();

    for (const auto& target : targets) {
        QVariantList sessions;
        for (const auto& agent : target.Agents)
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
        map["os"]       = osToString(target.Os);

        list[QString::number(target.TargetId)] = map;
    }

    return this->scriptEngine->engine()->toScriptValue(list);
}

void BridgeApp::targets_add(const QString &computer, const QString &domain, const QString &address, const QString &os, const QString &osDesc, const QString &tag, const QString &info, bool alive)
{
    TargetData target = {0, computer, domain, address, tag, QIcon(), 0, osDesc, "", 0, info, alive};

    target.Os = stringToOs(os);

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
        if (map.contains("os"))
            td.Os = stringToOs(map["os"].toString());
        targets.append(td);
    }

    if (targets.isEmpty())
        return;

    scriptEngine->manager()->GetAdaptix()->TargetsDock->TargetsAdd(targets);
}

int BridgeApp::ticks() { return QDateTime::currentSecsSinceEpoch(); }

QStringList BridgeApp::tokenize(const QString &cmdline) const
{
    QStringList tokens;
    QString token;
    bool inQuotes = false;
    const int length = cmdline.size();

    for (int i = 0; i < length; ) {
        const QChar c = cmdline.at(i);

        if (c == QLatin1Char(' ') && !inQuotes) {
            if (!token.isEmpty()) {
                tokens.append(token);
                token.clear();
            }
            ++i;
            continue;
        }

        if (c == QLatin1Char('"')) {
            inQuotes = !inQuotes;
            ++i;
            continue;
        }

        if (c == QLatin1Char('\\')) {
            int numBS = 0;
            while (i < length && cmdline.at(i) == QLatin1Char('\\')) {
                ++numBS;
                ++i;
            }
            if (i < length && cmdline.at(i) == QLatin1Char('"')) {
                for (int j = 0; j < numBS / 2; ++j)
                    token.append(QLatin1Char('\\'));
                if (numBS % 2 == 0)
                    inQuotes = !inQuotes;
                else
                    token.append(QLatin1Char('"'));
                ++i;
            } else {
                for (int j = 0; j < numBS; ++j)
                    token.append(QLatin1Char('\\'));
            }
            continue;
        }

        token.append(c);
        ++i;
    }

    if (!token.isEmpty())
        tokens.append(token);

    return tokens;
}

QJSValue BridgeApp::tunnels()
{
    QVariantMap list;
    auto tunnels = scriptEngine->manager()->GetTunnels();

    for (const auto& tun : tunnels) {
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
        list[QString::number(tun.TunnelId)]  = map;
    }

    return this->scriptEngine->engine()->toScriptValue(list);
}

QJSValue BridgeApp::validate_command(const QString &id, const QString &command) const
{
    auto mapAgents = scriptEngine->manager()->GetAgents();
    QVariantMap result;

    if (!mapAgents.contains(id.toLongLong())) {
        result["valid"] = false;
        result["message"] = "Agent not found";
        return scriptEngine->engine()->toScriptValue(result);
    }

    auto cmdResult = mapAgents[id.toLongLong()]->commander->ProcessInput(id.toLongLong(), command);
    result["valid"]            = !cmdResult.error;
    result["message"]       = cmdResult.message;
    result["is_pre_hook"]   = cmdResult.is_pre_hook;
    result["has_output"]    = cmdResult.output;
    result["has_post_hook"] = cmdResult.post_hook.isSet;
    result["has_handler"]   = cmdResult.handler.isSet;
    if (!cmdResult.error)
        result["parsed"] = cmdResult.data.toVariantMap();

    return scriptEngine->engine()->toScriptValue(result);
}
