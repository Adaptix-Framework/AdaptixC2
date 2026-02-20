#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <Agent/Agent.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/AxScriptWorker.h>
#include <Client/AxScript/AxUiFactory.h>
#include <Client/AxScript/BridgeEvent.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/SessionsTableWidget.h>
#include <UI/Widgets/AxConsoleWidget.h>

namespace {

QString taskTypeToString(int type) {
    switch (type) {
        case 1:  return "TASK";
        case 3:  return "JOB";
        case 4:  return "TUNNEL";
        default: return "unknown";
    }
}

} // namespace

AxScriptManager::AxScriptManager(AdaptixWidget* main_widget, QObject *parent): QObject(parent), adaptixWidget(main_widget) {
    uiFactory = new AxUiFactory(this);
    mainScript = new AxScriptEngine(this, "main", this);
}

AxScriptManager::~AxScriptManager()
{
    delete uiFactory;
}

QJSEngine* AxScriptManager::MainScriptEngine() { return mainScript ? mainScript->engine() : nullptr; }

AxUiFactory* AxScriptManager::GetUiFactory() const { return uiFactory; }

void AxScriptManager::Clear()
{
    if (mainScript) {
        auto commanderList = adaptixWidget->GetCommandersAll();
        for (const auto& commander : commanderList)
            commander->RemoveClientGroup(mainScript->context.name);

        delete mainScript;
        mainScript = nullptr;
    }

    for (auto &entry : config_scripts)
        delete entry.engine;
    config_scripts.clear();
    qDeleteAll(scripts);
    scripts.clear();
    qDeleteAll(server_scripts);
    server_scripts.clear();
}

void AxScriptManager::ResetMain()
{
    auto commanderList = adaptixWidget->GetCommandersAll();
    for (const auto& commander : commanderList)
        commander->RemoveClientGroup(mainScript->context.name);

    if (mainScript)
        delete mainScript;

    mainScript = new AxScriptEngine(this, "main", this);
}

QJSEngine* AxScriptManager::GetEngine(const QString &name)
{
    if (config_scripts.contains(name) && config_scripts[name].engine)
        return config_scripts[name].engine->engine();

    if (scripts.contains(name) && scripts[name])
        return scripts[name]->engine();

    if (name == "main" && mainScript)
        return mainScript->engine();

    return nullptr;
}

AdaptixWidget* AxScriptManager::GetAdaptix() const { return adaptixWidget; }

QMap<QString, Agent*> AxScriptManager::GetAgents() const {
    QReadLocker locker(&adaptixWidget->AgentsMapLock);
    return adaptixWidget->AgentsMap;
}

QVector<CredentialData> AxScriptManager::GetCredentials() const {
    QReadLocker locker(&adaptixWidget->CredentialsLock);
    return adaptixWidget->Credentials;
}

QVector<ListenerData> AxScriptManager::GetListeners() const {
    return adaptixWidget->Listeners;
}

QMap<QString, DownloadData> AxScriptManager::GetDownloads() const {
    QReadLocker locker(&adaptixWidget->DownloadsLock);
    return adaptixWidget->Downloads;
}

QMap<QString, ScreenData> AxScriptManager::GetScreenshots() const {
    QReadLocker locker(&adaptixWidget->ScreenshotsLock);
    return adaptixWidget->Screenshots;
}

QVector<TargetData> AxScriptManager::GetTargets() const {
    QReadLocker locker(&adaptixWidget->TargetsLock);
    return adaptixWidget->Targets;
}

QVector<TunnelData> AxScriptManager::GetTunnels() const {
    QReadLocker locker(&adaptixWidget->TunnelsLock);
    return adaptixWidget->Tunnels;
}

QStringList AxScriptManager::GetInterfaces() const { return adaptixWidget->addresses; }

/// CONFIG SCRIPTS (Listener, Agent, Service)

static QStringList configScriptListByType(const QMap<QString, ConfigScriptEntry> &map, ConfigScriptType type)
{
    QStringList result;
    for (auto it = map.begin(); it != map.end(); ++it) {
        if (it.value().type == type)
            result.append(it.key());
    }
    return result;
}

QStringList AxScriptManager::ListenerScriptList() { return configScriptListByType(config_scripts, ConfigScriptType::Listener); }

void AxScriptManager::ListenerScriptAdd(const QString &name, const QString &ax_script)
{
    if (config_scripts.contains(name))
        return;

    AxScriptEngine* script = new AxScriptEngine(this, name, this);
    script->execute(ax_script);
    config_scripts[name] = {ConfigScriptType::Listener, script};
}

QJSEngine* AxScriptManager::ListenerScriptEngine(const QString &name)
{
    if (!config_scripts.contains(name) || config_scripts[name].type != ConfigScriptType::Listener)
        return nullptr;
    return config_scripts[name].engine->engine();
}

QStringList AxScriptManager::AgentScriptList() { return configScriptListByType(config_scripts, ConfigScriptType::Agent); }

void AxScriptManager::AgentScriptAdd(const QString &name, const QString &ax_script)
{
    if (config_scripts.contains(name))
        return;

    AxScriptEngine* script = new AxScriptEngine(this, name, this);
    script->execute(ax_script);
    config_scripts[name] = {ConfigScriptType::Agent, script};
}

QJSEngine* AxScriptManager::AgentScriptEngine(const QString &name)
{
    if (!config_scripts.contains(name) || config_scripts[name].type != ConfigScriptType::Agent)
        return nullptr;
    return config_scripts[name].engine->engine();
}

QStringList AxScriptManager::ServiceScriptList() { return configScriptListByType(config_scripts, ConfigScriptType::Service); }

void AxScriptManager::ServiceScriptAdd(const QString &name, const QString &ax_script)
{
    if (config_scripts.contains(name))
        return;

    AxScriptEngine* script = new AxScriptEngine(this, name, this);
    script->execute(ax_script);
    config_scripts[name] = {ConfigScriptType::Service, script};

    QJSValue func = script->engine()->globalObject().property("InitService");
    if (func.isCallable())
        func.call();
}

QJSEngine* AxScriptManager::ServiceScriptEngine(const QString &name)
{
    if (!config_scripts.contains(name) || config_scripts[name].type != ConfigScriptType::Service)
        return nullptr;
    return config_scripts[name].engine->engine();
}

void AxScriptManager::ServiceScriptDataHandler(const QString &name, const QString &data)
{
    if (!config_scripts.contains(name) || config_scripts[name].type != ConfigScriptType::Service)
        return;

    QJSValue func = config_scripts[name].engine->engine()->globalObject().property("data_handler");
    if (!func.isCallable())
        return;

    func.call(QJSValueList() << QJSValue(data));
}

QJSValue AxScriptManager::AgentScriptExecute(const QString &name, const QString &code)
{
    QJSValue result;
    if (config_scripts.contains(name) && config_scripts[name].type == ConfigScriptType::Agent) {
        QJSValue func = config_scripts[name].engine->engine()->globalObject().property(code);
        if (func.isCallable()) {
            QJSValueList args;
            args << QJSValue("BeaconHTTP");
            result = func.call(args);
        }
    }
    return result;
}



QStringList AxScriptManager::ScriptList() { return scripts.keys(); }

bool AxScriptManager::ScriptAdd(ExtensionFile* ext)
{
    AxScriptEngine* script = new AxScriptEngine(this, ext->FilePath, this);
    scripts[ext->FilePath] = script;
    bool result = script->execute(ext->Code);
    if (result) {
        QJSValue metadata = script->engine()->globalObject().property("metadata");
        if (metadata.isObject()) {
            ext->Name = metadata.property("name").toString();
            ext->Description = metadata.property("description").toString();
            ext->NoSave = metadata.property("nosave").toBool();
        }
    }
    else {
        ext->Enabled = false;
        ext->Message = QString("%1\n    at line %2 in %3").arg(script->context.scriptObject.toString()).arg(script->context.scriptObject.property("lineNumber").toInt()).arg(ext->FilePath);
    }
    return result;
}

void AxScriptManager::ScriptRemove(const ExtensionFile &ext)
{
    auto scriptEngine = scripts.take(ext.FilePath);

    auto commanderList = adaptixWidget->GetCommandersAll();
    for (const auto& commander : commanderList)
        commander->RemoveClientGroup(ext.FilePath);

    delete scriptEngine;
}

void AxScriptManager::ServerScriptAdd(const ServerScriptData &data)
{
    if (server_scripts.contains(data.name)) {
        delete server_scripts.take(data.name);
    }

    ServerScriptData scriptData = data;
    scriptData.enabled = true;

    if (!data.code.isEmpty()) {
        AxScriptEngine* scriptEngine = new AxScriptEngine(this, "__server__:" + data.name, this);
        scriptEngine->setServerMode(true);
        scriptEngine->execute(data.code);
        server_scripts[data.name] = scriptEngine;

        QJSValue metadata = scriptEngine->engine()->globalObject().property("metadata");
        if (metadata.isObject() && scriptData.description.isEmpty())
            scriptData.description = metadata.property("description").toString();
    }

    server_scripts_data[data.name] = scriptData;
}

void AxScriptManager::ServerScriptRemove(const QString &name)
{
    if (server_scripts.contains(name)) {
        delete server_scripts.take(name);
    }
    server_scripts_data.remove(name);
}

void AxScriptManager::ServerScriptSetEnabled(const QString &name, bool enabled)
{
    if (!server_scripts_data.contains(name))
        return;

    if (server_scripts_data[name].enabled == enabled)
        return;

    server_scripts_data[name].enabled = enabled;

    if (enabled) {
        if (server_scripts.contains(name))
            return;

        const QString &code = server_scripts_data[name].code;
        if (!code.isEmpty()) {
            AxScriptEngine* scriptEngine = new AxScriptEngine(this, "__server__:" + name, this);
            scriptEngine->setServerMode(true);
            scriptEngine->execute(code);
            server_scripts[name] = scriptEngine;
        }
    } else {
        if (server_scripts.contains(name)) {
            delete server_scripts.take(name);
        }
    }
}

bool AxScriptManager::ServerScriptIsEnabled(const QString &name) const
{
    if (!server_scripts_data.contains(name))
        return false;
    return server_scripts_data[name].enabled;
}

QJSEngine* AxScriptManager::ServerScriptEngine(const QString &name)
{
    if (!server_scripts.contains(name))
        return nullptr;
    return server_scripts[name]->engine();
}

QList<ServerScriptData> AxScriptManager::ServerScriptList() const
{
    return server_scripts_data.values();
}

ServerScriptData AxScriptManager::ServerScriptGet(const QString &name) const
{
    return server_scripts_data.value(name);
}

void AxScriptManager::GlobalScriptLoad(const QString &path) { Q_EMIT adaptixWidget->LoadGlobalScriptSignal(path); }

void AxScriptManager::GlobalScriptUnload(const QString &path) { Q_EMIT adaptixWidget->UnloadGlobalScriptSignal(path); }

void AxScriptManager::GlobalScriptLoadAsync(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        consolePrintError("Could not open script: " + path);
        return;
    }
    QTextStream in(&file);
    QString code = in.readAll();
    file.close();

    ExecuteAsync(code, path);
}

void AxScriptManager::ExecuteAsync(const QString& code, const QString& name)
{
    auto* worker = new AxScriptWorker(this, name, nullptr);
    
    connect(worker, &AxScriptWorker::executionFinished, this, [this, worker, name](bool success, const QString& error) {
        if (!success && !error.isEmpty()) {
            consolePrintError(QString("Async script '%1' error: %2").arg(name).arg(error));
        }
        worker->deleteLater();
    });

    worker->waitForReady();
    worker->executeAsync(code);
}

bool AxScriptManager::containsUiCalls(const QString& code)
{
    static const QRegularExpression uiRegex(
        QStringLiteral("\\b(form\\.create_|form\\.connect|ax\\.dialog|ax\\.input|"
                       "ax\\.message|ax\\.confirm|ax\\.file_dialog|ax\\.color_dialog|"
                       "ax\\.font_dialog|menu\\.add_|menu\\.create_)")
    );
    return uiRegex.match(code).hasMatch();
}

void AxScriptManager::ExecuteSmart(const QString& code, const QString& name)
{
    if (containsUiCalls(code)) {
        AxScriptEngine* script = new AxScriptEngine(this, name, this);
        script->execute(code);
        delete script;
    } else {
        ExecuteAsync(code, name);
    }
}

QList<AxScriptEngine*> AxScriptManager::getAllEngines() const
{
    QList<AxScriptEngine*> list;
    list.reserve(config_scripts.size() + scripts.size() + server_scripts.size() + 1);

    for (const auto &entry : config_scripts)
        list << entry.engine;

    list << scripts.values() << server_scripts.values();

    if (mainScript)
        list << mainScript;
    return list;
}

QList<AxMenuItem> AxScriptManager::collectMenuItems(const QString &menuType) const
{
    QList<AxMenuItem> items;
    for (const auto& script : getAllEngines()) {
        if (script)
            items += script->getMenuItems(menuType);
    }
    return items;
}

QList<AxEvent> AxScriptManager::collectEvents(const QString &eventType) const
{
    QList<AxEvent> items;
    for (const auto& script : getAllEngines()) {
        if (script)
            items += script->getEvents(eventType);
    }
    return items;
}

void AxScriptManager::safeCallHandler(const AxEvent& event, const QJSValueList& args)
{
    if (!event.jsEngine || !event.handler.isCallable())
        return;

    QJSValue result = event.handler.call(args);
    if (result.isError()) {
        QString error = QString("Script error in event handler: %1\n    at line %2\n    stack: %3")
            .arg(result.toString())
            .arg(result.property("lineNumber").toInt())
            .arg(result.property("stack").toString());
        consolePrintError(error);
    }
}

int AxScriptManager::addMenuItemsToMenu(QMenu* menu, const QList<AxMenuItem>& items, const QVariantList& context)
{
    int count = 0;
    for (const auto& item : items) {
        item.menu->setContext(context);
        if (auto* sep = dynamic_cast<AxSeparatorWrapper*>(item.menu))
            menu->addAction(sep->action());
        else if (auto* act = dynamic_cast<AxActionWrapper*>(item.menu))
            menu->addAction(act->action());
        else if (auto* sub = dynamic_cast<AxMenuWrapper*>(item.menu))
            menu->addMenu(sub->menu());
        else
            continue;
        count++;
    }
    return count;
}

void AxScriptManager::RegisterCommandsGroup(const CommandsGroup &group, const QStringList &listeners, const QStringList &agents, const QList<int> &os)
{
    adaptixWidget->AddCommandsToCommanders(group, listeners, agents, os);
}

QStringList AxScriptManager::EventList()
{
    QStringList slist;
    for (const auto& script : getAllEngines()) {
        if (script)
            slist += script->listEvent();
    }
    return slist;
}

void AxScriptManager::EventRemove(const QString &event_id)
{
    for (const auto& script : getAllEngines()) {
        if (script)
            script->removeEvent(event_id);
    }
}

QList<AxMenuItem> AxScriptManager::FilterMenuItems(const QStringList &agentIds, const QString &menuType, const bool &agentsNeed)
{
    QSet<QString> agentTypes;
    QSet<QString> listenerTypes;
    QSet<int>     osTypes;
    {
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        for (const auto& agent_id: agentIds) {
            if (adaptixWidget->AgentsMap.contains(agent_id)) {
                const auto& agent = adaptixWidget->AgentsMap[agent_id];
                agentTypes.insert(agent->data.Name);
                osTypes.insert(agent->data.Os);
                listenerTypes.insert(agent->listenerType);
            }
        }
    }

    QList<AxMenuItem> items;
    for (const auto& script : getAllEngines()) {
        if (script)
            items += script->getMenuItems(menuType);
    }

    QList<AxMenuItem> ret;
    for (const auto& item : items) {
        if (agentsNeed) {
            if (!item.agents.contains(agentTypes))
                continue;
        } else {
            if (item.agents.size() > 0 && !item.agents.contains(agentTypes))
                continue;
        }
        if (item.os.size() > 0 && !item.os.contains(osTypes))
            continue;
        if (item.listeners.size() > 0 && !item.listeners.contains(listenerTypes))
            continue;

        ret.append(item);
    }
    return ret;
}

QList<AxEvent> AxScriptManager::FilterEvents(const QString &agentId, const QString &eventType)
{
    QList<AxEvent> ret;

    QString agentType;
    QString listenerType;
    int     osType;
    {
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        if (!adaptixWidget->AgentsMap.contains(agentId))
            return ret;

        agentType    = adaptixWidget->AgentsMap[agentId]->data.Name;
        listenerType = adaptixWidget->AgentsMap[agentId]->listenerType;
        osType       = adaptixWidget->AgentsMap[agentId]->data.Os;
    }

    QList<AxEvent> items;
    for (const auto& script : getAllEngines()) {
        if (script)
            items += script->getEvents(eventType);
    }

    for (const auto& event : items) {
        if (!event.agents.contains(agentType))
            continue;
        if (event.os.size() > 0 && !event.os.contains(osType))
            continue;
        if (event.listeners.size() > 0 && !event.listeners.contains(listenerType))
            continue;

        ret.append(event);
    }
    return ret;
}

void AxScriptManager::AppAgentHide(const QStringList &agents)
{
    bool updated = false;
    {
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        for (const auto& agentId : agents) {
            if (adaptixWidget->AgentsMap.contains(agentId)) {
                adaptixWidget->AgentsMap[agentId]->show = false;
                updated = true;
            }
        }
    }

    if (updated)
        adaptixWidget->SessionsTableDock->UpdateData();
}

void AxScriptManager::AppAgentRemove(const QStringList &agents)
{
    HttpReqAgentRemoveAsync(agents, *(adaptixWidget->GetProfile()), nullptr);
}

/// APP

void AxScriptManager::AppAgentSetColor(const QStringList &agents, const QString &background, const QString &foreground, const bool reset)
{
    HttpReqAgentSetColorAsync(agents, background, foreground, reset, *(adaptixWidget->GetProfile()), nullptr);
}

void AxScriptManager::AppAgentSetMark(const QStringList &agents, const QString &mark)
{
    HttpReqAgentSetMarkAsync(agents, mark, *(adaptixWidget->GetProfile()), nullptr);
}

void AxScriptManager::AppAgentSetTag(const QStringList &agents, const QString &tag)
{
    HttpReqAgentSetTagAsync(agents, tag, *(adaptixWidget->GetProfile()), nullptr);
}

void AxScriptManager::AppAgentUpdateData(const QString &id, const QJsonObject &updateData)
{
    HttpReqAgentUpdateDataAsync(id, updateData, *(adaptixWidget->GetProfile()), nullptr);
}

/// MENU

int AxScriptManager::AddMenuSession(QMenu *menu, const QString &menuType, QStringList agentIds)
{
    QVariantList context;
    {
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        for (const auto& agent_id: agentIds) {
            if (adaptixWidget->AgentsMap.contains(agent_id))
                context << agent_id;
        }
    }
    return addMenuItemsToMenu(menu, FilterMenuItems(agentIds, menuType, true), context);
}

int AxScriptManager::AddMenuFileBrowser(QMenu *menu, QVector<DataMenuFileBrowser> files)
{
    if (files.empty()) return 0;

    QVariantList context;
    {
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        for (const auto& file : files) {
            if (adaptixWidget->AgentsMap.contains(file.agentId)) {
                QVariantMap map;
                map["agent_id"] = file.agentId;
                map["path"]     = file.path;
                map["name"]     = file.name;
                map["type"]     = file.type;
                context << map;
            }
        }
    }
    return addMenuItemsToMenu(menu, FilterMenuItems(QStringList() << files[0].agentId, "FileBrowser", true), context);
}

int AxScriptManager::AddMenuProcessBrowser(QMenu *menu, QVector<DataMenuProcessBrowser> processes)
{
    if (processes.empty()) return 0;

    QVariantList context;
    {
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        for (const auto& proc : processes) {
            if (adaptixWidget->AgentsMap.contains(proc.agentId)) {
                QVariantMap map;
                map["agent_id"]   = proc.agentId;
                map["pid"]        = proc.pid;
                map["ppid"]       = proc.ppid;
                map["arch"]       = proc.arch;
                map["session_id"] = proc.session_id;
                map["context"]    = proc.context;
                map["process"]    = proc.process;
                context << map;
            }
        }
    }
    return addMenuItemsToMenu(menu, FilterMenuItems(QStringList() << processes[0].agentId, "ProcessBrowser", true), context);
}

int AxScriptManager::AddMenuDownload(QMenu *menu, const QString &menuType, QVector<DataMenuDownload> files, const bool &agnetNeed)
{
    if (files.empty()) return 0;

    QVariantList context;
    {
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        for (const auto& file : files) {
            QVariantMap map;
            map["agent_id"] = file.agentId;
            map["file_id"]  = file.fileId;
            map["path"]     = file.path;
            map["state"]    = file.state;
            context << map;
        }
    }
    return addMenuItemsToMenu(menu, FilterMenuItems(QStringList() << files[0].agentId, menuType, false), context);
}

int AxScriptManager::AddMenuTask(QMenu *menu, const QString &menuType, const QStringList &tasks)
{
    if (tasks.empty()) return 0;

    QSet<QString> agents;
    QVariantList context;
    for (const auto& taskId : tasks) {
        if (adaptixWidget->TasksMap.contains(taskId)) {
            TaskData taskData = adaptixWidget->TasksMap[taskId];
            QVariantMap map;
            map["agent_id"] = taskData.AgentId;
            map["task_id"]  = taskData.TaskId;
            map["state"]    = taskData.Status;
            map["type"] = taskTypeToString(taskData.TaskType);

            context << map;
            agents.insert(taskData.AgentId);
        }
    }
    return addMenuItemsToMenu(menu, FilterMenuItems(QList<QString>(agents.begin(), agents.end()), menuType, true), context);
}

int AxScriptManager::AddMenuTargets(QMenu *menu, const QString &menuType, const QStringList &targets)
{
    QVariantList context;
    for (const auto& targetId: targets)
        context << targetId;
    return addMenuItemsToMenu(menu, collectMenuItems(menuType), context);
}

int AxScriptManager::AddMenuCreds(QMenu *menu, const QString &menuType, const QStringList &creds)
{
    QVariantList context;
    for (const auto& credId: creds)
        context << credId;
    return addMenuItemsToMenu(menu, collectMenuItems(menuType), context);
}


/// EVENT

void AxScriptManager::emitNewAgent(const QString &agentId)
{
    for (const auto& event : FilterEvents(agentId, "new_agent")) {
        if (event.jsEngine) {
            QJSValue argId = event.jsEngine->toScriptValue(agentId);
            safeCallHandler(event, QJSValueList() << argId);
        }
    }
}

void AxScriptManager::emitFileBrowserDisks(const QString &agentId)
{
    for (const auto& event : FilterEvents(agentId, "FileBrowserDisks")) {
        if (event.jsEngine) {
            QJSValue argId = event.jsEngine->toScriptValue(agentId);
            safeCallHandler(event, QJSValueList() << argId);
        }
    }
}

void AxScriptManager::emitFileBrowserList(const QString &agentId, const QString &path)
{
    for (const auto& event : FilterEvents(agentId, "FileBrowserList")) {
        if (event.jsEngine) {
            QJSValue argId   = event.jsEngine->toScriptValue(agentId);
            QJSValue argPath = event.jsEngine->toScriptValue(path);
            safeCallHandler(event, QJSValueList() << argId << argPath);
        }
    }
}

void AxScriptManager::emitFileBrowserUpload(const QString &agentId, const QString &path, const QString &localFilename)
{
    for (const auto& event : FilterEvents(agentId, "FileBrowserUpload")) {
        if (event.jsEngine) {
            QJSValue argId   = event.jsEngine->toScriptValue(agentId);
            QJSValue argPath = event.jsEngine->toScriptValue(path);
            QJSValue argFile = event.jsEngine->toScriptValue(localFilename);
            safeCallHandler(event, QJSValueList() << argId << argPath << argFile);
        }
    }
}

void AxScriptManager::emitProcessBrowserList(const QString &agentId)
{
    for (const auto& event : FilterEvents(agentId, "ProcessBrowserList")) {
        if (event.jsEngine) {
            QJSValue argId = event.jsEngine->toScriptValue(agentId);
            safeCallHandler(event, QJSValueList() << argId);
        }
    }
}

void AxScriptManager::emitReadyClient()
{
    for (const auto& event : collectEvents("ready"))
        safeCallHandler(event);
}

void AxScriptManager::emitDisconnectClient()
{
    for (const auto& event : collectEvents("disconnect"))
        safeCallHandler(event);
}

/// SLOTS

void AxScriptManager::consolePrintMessage(const QString &message) { this->adaptixWidget->AxConsoleDock->PrintMessage(message); }

void AxScriptManager::consolePrintError(const QString &message) { this->adaptixWidget->AxConsoleDock->PrintError(message); }
