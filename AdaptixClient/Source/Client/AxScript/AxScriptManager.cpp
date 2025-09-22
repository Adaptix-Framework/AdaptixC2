#include <Agent/Agent.h>
#include <Agent/Task.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/BridgeEvent.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/SessionsTableWidget.h>
#include <UI/Widgets/AxConsoleWidget.h>

AxScriptManager::AxScriptManager(AdaptixWidget* main_widget, QObject *parent): QObject(parent), adaptixWidget(main_widget) {
    mainScript = new AxScriptEngine(this, "main", this);
}

AxScriptManager::~AxScriptManager() = default;

QJSEngine* AxScriptManager::MainScriptEngine() { return mainScript->engine(); }

void AxScriptManager::Clear()
{
    ResetMain();

    qDeleteAll(agents_scripts);
    agents_scripts.clear();
    qDeleteAll(listeners_scripts);
    listeners_scripts.clear();
    qDeleteAll(scripts);
    scripts.clear();
}

void AxScriptManager::ResetMain()
{
    auto commanderList = adaptixWidget->GetCommandersAll();
    for (auto commander : commanderList)
        commander->RemoveAxCommands(mainScript->context.name);

    if (mainScript)
        delete mainScript;

    mainScript = new AxScriptEngine(this, "main", this);
}

QJSEngine* AxScriptManager::GetEngine(const QString &name)
{
    if (agents_scripts.contains(name) && agents_scripts[name])
        return agents_scripts[name]->engine();

    if (scripts.contains(name) && scripts[name])
        return scripts[name]->engine();

    if (name == "main" && mainScript)
        return mainScript->engine();

    return nullptr;
}

AdaptixWidget* AxScriptManager::GetAdaptix() const { return adaptixWidget; }

QMap<QString, Agent*> AxScriptManager::GetAgents() const { return adaptixWidget->AgentsMap; }

QVector<CredentialData> AxScriptManager::GetCredentials() const { return adaptixWidget->Credentials; }

QMap<QString, DownloadData> AxScriptManager::GetDownloads() const { return adaptixWidget->Downloads; }

QMap<QString, ScreenData> AxScriptManager::GetScreenshots() const { return adaptixWidget->Screenshots; }

QVector<TargetData> AxScriptManager::GetTargets() const { return adaptixWidget->Targets; }

QVector<TunnelData> AxScriptManager::GetTunnels() const { return adaptixWidget->Tunnels; }

QStringList AxScriptManager::GetInterfaces() const { return adaptixWidget->addresses; }

/// MAIN

QStringList AxScriptManager::ListenerScriptList() { return listeners_scripts.keys(); }

void AxScriptManager::ListenerScriptAdd(const QString &name, const QString &ax_script)
{
    if (listeners_scripts.contains(name))
        return;

    AxScriptEngine* script = new AxScriptEngine(this, name, this);
    script->execute(ax_script);

    listeners_scripts[name] = script;
}

QJSEngine* AxScriptManager::ListenerScriptEngine(const QString &name)
{
    if (!listeners_scripts.contains(name)) return nullptr;
    return listeners_scripts[name]->engine();
}



QStringList AxScriptManager::AgentScriptList() { return agents_scripts.keys(); }

void AxScriptManager::AgentScriptAdd(const QString &name, const QString &ax_script)
{
    if (agents_scripts.contains(name)) return;

    AxScriptEngine* script = new AxScriptEngine(this, name, this);
    script->execute(ax_script);

    agents_scripts[name] = script;
}

QJSEngine* AxScriptManager::AgentScriptEngine(const QString &name)
{
    if (!agents_scripts.contains(name)) return nullptr;
    return agents_scripts[name]->engine();
}

QJSValue AxScriptManager::AgentScriptExecute(const QString &name, const QString &code)
{
    QJSValue result;
    if (agents_scripts.contains(name)) {
        QJSValue func = agents_scripts[name]->engine()->globalObject().property(code);
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
    for (auto commander : commanderList)
        commander->RemoveAxCommands(ext.FilePath);

    delete scriptEngine;
}



void AxScriptManager::GlobalScriptLoad(const QString &path) { Q_EMIT adaptixWidget->LoadGlobalScriptSignal(path); }

void AxScriptManager::GlobalScriptUnload(const QString &path) { Q_EMIT adaptixWidget->UnloadGlobalScriptSignal(path); }

void AxScriptManager::RegisterCommandsGroup(const CommandsGroup &group, const QStringList &listeners, const QStringList &agents, const QList<int> &os)
{
    auto commanderList = adaptixWidget->GetCommanders(listeners, agents, os);
    for (auto commander : commanderList)
        commander->AddAxCommands(group);
}

QStringList AxScriptManager::EventList()
{
    QStringList slist;
    QList<AxScriptEngine*> list = this->agents_scripts.values() + this->scripts.values();
    list.append(this->mainScript);

    for (const auto script : list) {
        if (script != nullptr)
            slist += script->listEvent();
    }
    return slist;
}

void AxScriptManager::EventRemove(const QString &event_id)
{
    QList<AxScriptEngine*> list = this->agents_scripts.values() + this->scripts.values();
    list.append(this->mainScript);

    for (const auto script : list)
        script->removeEvent(event_id);
}

QList<AxMenuItem> AxScriptManager::FilterMenuItems(const QStringList &agentIds, const QString &menuType)
{
    QSet<QString> agentTypes;
    QSet<QString> listenerTypes;
    QSet<int>     osTypes;
    for (auto agent_id: agentIds) {
        if (adaptixWidget->AgentsMap.contains(agent_id)) {
            agentTypes.insert(adaptixWidget->AgentsMap[agent_id]->data.Name);
            osTypes.insert(adaptixWidget->AgentsMap[agent_id]->data.Os);
            listenerTypes.insert(adaptixWidget->AgentsMap[agent_id]->listenerType);
        }
    }

    QList<AxScriptEngine*> list = this->agents_scripts.values() + this->scripts.values();
    list.append(this->mainScript);

    QList<AxMenuItem> items;
    for (const auto script : list)
        items += script->getMenuItems(menuType);

    QList<AxMenuItem> ret;
    for (int i = 0; i < items.size(); ++i) {
        AxMenuItem item = items[i];

        if ( !item.agents.contains(agentTypes) )
            continue;
        if (item.os.size() > 0 && !item.os.contains(osTypes))
            continue;
        if (item.listenerts.size() > 0 && !item.listenerts.contains(listenerTypes))
            continue;

        ret.append(item);
    }
    return ret;
}

QList<AxEvent> AxScriptManager::FilterEvents(const QString &agentId, const QString &eventType)
{
    QList<AxEvent> ret;

    if( !adaptixWidget->AgentsMap.contains(agentId) )
        return ret;

    QString agentType    = adaptixWidget->AgentsMap[agentId]->data.Name;
    QString listenerType = adaptixWidget->AgentsMap[agentId]->listenerType;
    int     osType          = adaptixWidget->AgentsMap[agentId]->data.Os;

    QList<AxScriptEngine*> list = this->agents_scripts.values() + this->scripts.values();
    list.append(this->mainScript);

    QList<AxEvent> items;
    for (const auto script : list) {
        items += script->getEvents(eventType);
    }

    for (int i = 0; i < items.size(); ++i) {
        AxEvent event = items[i];

        if ( !event.agents.contains(agentType) )
            continue;
        if (event.os.size() > 0 && !event.os.contains(osType))
            continue;
        if (event.listenerts.size() > 0 && !event.listenerts.contains(listenerType))
            continue;

        ret.append(event);
    }
    return ret;
}

void AxScriptManager::AppAgentHide(const QStringList &agents)
{
    bool updated = false;
    for (auto agentId : agents) {
        if (adaptixWidget->AgentsMap.contains(agentId)) {
            adaptixWidget->AgentsMap[agentId]->show = false;
            updated = true;
        }
    }

    if (updated)
        adaptixWidget->SessionsTablePage->SetData();
}

void AxScriptManager::AppAgentRemove(const QStringList &agents)
{
    QString message = "";
    bool ok = false;
    HttpReqAgentRemove(agents, *(adaptixWidget->GetProfile()), &message, &ok);
}

/// APP

void AxScriptManager::AppAgentSetColor(const QStringList &agents, const QString &background, const QString &foreground, const bool reset)
{
    QString message = "";
    bool ok = false;
    HttpReqAgentSetColor(agents, background, foreground, reset, *(adaptixWidget->GetProfile()), &message, &ok);
}

void AxScriptManager::AppAgentSetImpersonate(const QString &id, const QString &impersonate, const bool elevated)
{
    QString message = "";
    bool ok = false;
    HttpReqAgentSetImpersonate(id, impersonate, elevated, *(adaptixWidget->GetProfile()), &message, &ok);
}

void AxScriptManager::AppAgentSetMark(const QStringList &agents, const QString &mark)
{
    QString message = "";
    bool ok = false;
    HttpReqAgentSetMark(agents, mark, *(adaptixWidget->GetProfile()), &message, &ok);
}

void AxScriptManager::AppAgentSetTag(const QStringList &agents, const QString &tag)
{
    QString message = "";
    bool ok = false;
    HttpReqAgentSetTag(agents, tag, *(adaptixWidget->GetProfile()), &message, &ok);
}

/// MENU

int AxScriptManager::AddMenuSession(QMenu *menu, const QString &menuType, QStringList agentIds)
{
    QVariantList context;
    for (auto agent_id: agentIds) {
        if (adaptixWidget->AgentsMap.contains(agent_id)) {
            context << agent_id;
        }
    }
    int count = 0;
    QList<AxMenuItem> items = this->FilterMenuItems(agentIds, menuType);
    for (int i = 0; i < items.size(); ++i) {
        AxMenuItem item = items[i];

        item.menu->setContext(context);
        if (auto* item1 = dynamic_cast<AxSeparatorWrapper*>(item.menu))
            menu->addAction(item1->action());
        else if (auto* item2 = dynamic_cast<AxActionWrapper*>(item.menu))
            menu->addAction(item2->action());
        else if (auto* item3 = dynamic_cast<AxMenuWrapper*>(item.menu))
            menu->addMenu(item3->menu());
        else
            continue;
        count++;
    }
    return count;
}

int AxScriptManager::AddMenuFileBrowser(QMenu *menu, QVector<DataMenuFileBrowser> files)
{
    if (files.empty()) return 0;

    QVariantList  context;
    for (auto file : files) {
        if (adaptixWidget->AgentsMap.contains(file.agentId)) {
            QVariantMap map;
            map["agent_id"] = file.agentId;
            map["path"]     = file.path;
            map["name"]     = file.name;
            map["type"]     = file.type;
            context << map;
        }
    }

    int count = 0;
    QList<AxMenuItem> items = this->FilterMenuItems(QStringList() << files[0].agentId, "FileBrowser");
    for (int i = 0; i < items.size(); ++i) {
        AxMenuItem item = items[i];

        item.menu->setContext(context);
        if (auto* item1 = dynamic_cast<AxSeparatorWrapper*>(item.menu))
            menu->addAction(item1->action());
        else if (auto* item2 = dynamic_cast<AxActionWrapper*>(item.menu))
            menu->addAction(item2->action());
        else if (auto* item3 = dynamic_cast<AxMenuWrapper*>(item.menu))
            menu->addMenu(item3->menu());
        else
            continue;
        count++;
    }
    return count;
}

int AxScriptManager::AddMenuProcessBrowser(QMenu *menu, QVector<DataMenuProcessBrowser> processes)
{
    if (processes.empty()) return 0;

    QVariantList  context;
    for (auto proc : processes) {
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

    int count = 0;
    QList<AxMenuItem> items = this->FilterMenuItems(QStringList() << processes[0].agentId, "ProcessBrowser");
    for (int i = 0; i < items.size(); ++i) {
        AxMenuItem item = items[i];

        item.menu->setContext(context);
        if (auto* item1 = dynamic_cast<AxSeparatorWrapper*>(item.menu))
            menu->addAction(item1->action());
        else if (auto* item2 = dynamic_cast<AxActionWrapper*>(item.menu))
            menu->addAction(item2->action());
        else if (auto* item3 = dynamic_cast<AxMenuWrapper*>(item.menu))
            menu->addMenu(item3->menu());
        else
            continue;
        count++;
    }
    return count;
}

int AxScriptManager::AddMenuDownload(QMenu *menu, const QString &menuType, QVector<DataMenuDownload> files)
{
    if (files.empty()) return 0;

    QVariantList context;
    for (auto file : files) {
        if (adaptixWidget->AgentsMap.contains(file.agentId)) {
            QVariantMap map;
            map["agent_id"] = file.agentId;
            map["file_id"]  = file.fileId;
            map["path"]     = file.path;
            map["state"]    = file.state;
            context << map;
        }
    }

    int count = 0;
    QList<AxMenuItem> items = this->FilterMenuItems(QStringList() << files[0].agentId, menuType);
    for (int i = 0; i < items.size(); ++i) {
        AxMenuItem item = items[i];

        item.menu->setContext(context);
        if (auto* item1 = dynamic_cast<AxSeparatorWrapper*>(item.menu))
            menu->addAction(item1->action());
        else if (auto* item2 = dynamic_cast<AxActionWrapper*>(item.menu))
            menu->addAction(item2->action());
        else if (auto* item3 = dynamic_cast<AxMenuWrapper*>(item.menu))
            menu->addMenu(item3->menu());
        else
            continue;
        count++;
    }
    return count;
}

int AxScriptManager::AddMenuTask(QMenu *menu, const QString &menuType, const QStringList &tasks)
{
    if (tasks.empty()) return 0;

    QSet<QString> agents;

    QVariantList context;
    for (auto taskId : tasks) {
        if (adaptixWidget->TasksMap.contains(taskId) && adaptixWidget->TasksMap[taskId]) {
            TaskData taskData = adaptixWidget->TasksMap[taskId]->data;
            QVariantMap map;
            map["agent_id"] = taskData.AgentId;
            map["task_id"]  = taskData.TaskId;
            map["state"]    = taskData.Status;
            if ( taskData.TaskType == 1 )
                map["type"] = "TASK";
            else if ( taskData.TaskType == 3 )
                map["type"] = "JOB";
            else if ( taskData.TaskType == 4 )
                map["type"] = "TUNNEL";
            else
                map["type"] = "unknown";

            context << map;
            agents.insert(taskData.AgentId);
        }
    }

    int count = 0;
    QList<AxMenuItem> items = this->FilterMenuItems(QList<QString>(agents.begin(), agents.end()), menuType);
    for (int i = 0; i < items.size(); ++i) {
        AxMenuItem item = items[i];

        item.menu->setContext(context);
        if (auto* item1 = dynamic_cast<AxSeparatorWrapper*>(item.menu))
            menu->addAction(item1->action());
        else if (auto* item2 = dynamic_cast<AxActionWrapper*>(item.menu))
            menu->addAction(item2->action());
        else if (auto* item3 = dynamic_cast<AxMenuWrapper*>(item.menu))
            menu->addMenu(item3->menu());
        else
            continue;
        count++;
    }
    return count;
}

int AxScriptManager::AddMenuTargets(QMenu *menu, const QString &menuType, const QStringList &targets)
{
    QVariantList context;
    for (auto targetId: targets)
        context << targetId;

    QList<AxScriptEngine*> list = this->agents_scripts.values() + this->scripts.values();
    list.append(this->mainScript);

    QList<AxMenuItem> items;
    for (const auto script : list)
        items += script->getMenuItems(menuType);

    int count = 0;
    for (int i = 0; i < items.size(); ++i) {
        AxMenuItem item = items[i];

        item.menu->setContext(context);
        if (auto* item1 = dynamic_cast<AxSeparatorWrapper*>(item.menu))
            menu->addAction(item1->action());
        else if (auto* item2 = dynamic_cast<AxActionWrapper*>(item.menu))
            menu->addAction(item2->action());
        else if (auto* item3 = dynamic_cast<AxMenuWrapper*>(item.menu))
            menu->addMenu(item3->menu());
        else
            continue;
        count++;
    }
    return count;
}

int AxScriptManager::AddMenuCreds(QMenu *menu, const QString &menuType, const QStringList &creds)
{
    QVariantList context;
    for (auto credId: creds)
        context << credId;

    QList<AxScriptEngine*> list = this->agents_scripts.values() + this->scripts.values();
    list.append(this->mainScript);

    QList<AxMenuItem> items;
    for (const auto script : list)
        items += script->getMenuItems(menuType);

    int count = 0;
    for (int i = 0; i < items.size(); ++i) {
        AxMenuItem item = items[i];

        item.menu->setContext(context);
        if (auto* item1 = dynamic_cast<AxSeparatorWrapper*>(item.menu))
            menu->addAction(item1->action());
        else if (auto* item2 = dynamic_cast<AxActionWrapper*>(item.menu))
            menu->addAction(item2->action());
        else if (auto* item3 = dynamic_cast<AxMenuWrapper*>(item.menu))
            menu->addMenu(item3->menu());
        else
            continue;
        count++;
    }
    return count;
}


/// EVENT

void AxScriptManager::emitNewAgent(const QString &agentId)
{
    QList<AxEvent> items = this->FilterEvents(agentId, "new_agent");
    for (int i = 0; i < items.size(); ++i) {
        AxEvent event = items[i];
        if (event.jsEngine) {
            QJSValue argId = event.jsEngine->toScriptValue(agentId);
            event.handler.call(QJSValueList() << argId);
        }
    }
}

void AxScriptManager::emitFileBrowserDisks(const QString &agentId)
{
    QList<AxEvent> items = this->FilterEvents(agentId, "FileBroserDisks");
    for (int i = 0; i < items.size(); ++i) {
       AxEvent event = items[i];
        if (event.jsEngine) {
            QJSValue argId = event.jsEngine->toScriptValue(agentId);
            event.handler.call(QJSValueList() << argId);
        }
    }
}

void AxScriptManager::emitFileBrowserList(const QString &agentId, const QString &path)
{
    QList<AxEvent> items = this->FilterEvents(agentId, "FileBroserList");
    for (int i = 0; i < items.size(); ++i) {
        AxEvent event = items[i];
        if (event.jsEngine) {
            QJSValue argId   = event.jsEngine->toScriptValue(agentId);
            QJSValue argPath = event.jsEngine->toScriptValue(path);
            event.handler.call(QJSValueList() << argId << argPath);
        }
    }
}

void AxScriptManager::emitFileBrowserUpload(const QString &agentId, const QString &path, const QString &localFilename)
{
    QList<AxEvent> items = this->FilterEvents(agentId, "FileBrowserUpload");
    for (int i = 0; i < items.size(); ++i) {
        AxEvent event = items[i];
        if (event.jsEngine) {
            QJSValue argId   = event.jsEngine->toScriptValue(agentId);
            QJSValue argPath = event.jsEngine->toScriptValue(path);
            QJSValue argFile = event.jsEngine->toScriptValue(localFilename);
            event.handler.call(QJSValueList() << argId << argPath << argFile);
        }
    }
}

void AxScriptManager::emitProcessBrowserList(const QString &agentId)
{
    QList<AxEvent> items = this->FilterEvents(agentId, "ProcessBrowserList");
    for (int i = 0; i < items.size(); ++i) {
        AxEvent event = items[i];
        if (event.jsEngine) {
            QJSValue argId = event.jsEngine->toScriptValue(agentId);
            event.handler.call(QJSValueList() << argId);
        }
    }
}

void AxScriptManager::emitReadyClient()
{
    QList<AxScriptEngine*> list = this->agents_scripts.values() + this->scripts.values();
    list.append(this->mainScript);

    QList<AxEvent> items;
    for (const auto script : list) {
        items += script->getEvents("ready");
    }

    for (int i = 0; i < items.size(); ++i) {
        AxEvent event = items[i];
        if (event.jsEngine)
            event.handler.call();
    }
}

void AxScriptManager::emitDisconnectClient()
{
    QList<AxScriptEngine*> list = this->agents_scripts.values() + this->scripts.values();
    list.append(this->mainScript);

    QList<AxEvent> items;
    for (const auto script : list) {
        items += script->getEvents("disconnect");
    }

    for (int i = 0; i < items.size(); ++i) {
        AxEvent event = items[i];
        if (event.jsEngine)
            event.handler.call();
    }
}

/// SLOTS

void AxScriptManager::consolePrintMessage(const QString &message) { this->adaptixWidget->AxConsoleTab->PrintMessage(message); }

void AxScriptManager::consolePrintError(const QString &message) { this->adaptixWidget->AxConsoleTab->PrintError(message); }
