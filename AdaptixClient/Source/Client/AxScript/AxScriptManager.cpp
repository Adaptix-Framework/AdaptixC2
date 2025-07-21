#include <Agent/Agent.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/BridgeEvent.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/AxConsoleWidget.h>

AxScriptManager::AxScriptManager(AdaptixWidget* main_widget, QObject *parent): QObject(parent), mainWidget(main_widget) {
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
    auto commanderList = mainWidget->GetCommandersAll();
    for (auto commander : commanderList)
        commander->RemoveAxCommands(mainScript->context.name);

    if (mainScript)
        delete mainScript;

    mainScript = new AxScriptEngine(this, "main", this);
}

AdaptixWidget* AxScriptManager::GetAdaptix() const { return this->mainWidget; }

QMap<QString, Agent*> AxScriptManager::GetAgents() const { return mainWidget->AgentsMap; }

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

    auto commanderList = mainWidget->GetCommandersAll();
    for (auto commander : commanderList)
        commander->RemoveAxCommands(ext.FilePath);

    delete scriptEngine;
}



void AxScriptManager::GlobalScriptLoad(const QString &path) { emit mainWidget->LoadGlobalScriptSignal(path); }

void AxScriptManager::GlobalScriptUnload(const QString &path) { emit mainWidget->UnloadGlobalScriptSignal(path); }

void AxScriptManager::RegisterCommandsGroup(const CommandsGroup &group, const QStringList &listeners, const QStringList &agents, const QList<int> &os)
{
    auto commanderList = mainWidget->GetCommanders(listeners, agents, os);
    for (auto commander : commanderList)
        commander->AddAxCommands(group);
}

void AxScriptManager::RemoveEvent(const QString &event_id)
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
        if (mainWidget->AgentsMap.contains(agent_id)) {
            agentTypes.insert(mainWidget->AgentsMap[agent_id]->data.Name);
            osTypes.insert(mainWidget->AgentsMap[agent_id]->data.Os);
            listenerTypes.insert(mainWidget->AgentsMap[agent_id]->listenerType);
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

    if( !mainWidget->AgentsMap.contains(agentId) )
        return ret;

    QString agentType    = mainWidget->AgentsMap[agentId]->data.Name;
    QString listenerType = mainWidget->AgentsMap[agentId]->listenerType;
    int     osType          = mainWidget->AgentsMap[agentId]->data.Os;

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


/// MENU

int AxScriptManager::AddMenuSession(QMenu *menu, const QString &menuType, QStringList agentIds)
{
    QVariantList context;
    for (auto agent_id: agentIds) {
        if (mainWidget->AgentsMap.contains(agent_id)) {
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
        if (mainWidget->AgentsMap.contains(file.agentId)) {
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

int AxScriptManager::AddMenuDownload(QMenu *menu, const QString &menuType, QVector<DataMenuDownload> files)
{
    if (files.empty()) return 0;

    QVariantList context;
    for (auto file : files) {
        if (mainWidget->AgentsMap.contains(file.agentId)) {
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

/// EVENT

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
    QList<AxEvent> items = this->FilterEvents(agentId, "FileBroserUpload");
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

/// SLOTS

void AxScriptManager::consolePrintMessage(const QString &message) { this->mainWidget->AxConsoleTab->PrintMessage(message); }

void AxScriptManager::consolePrintError(const QString &message) { this->mainWidget->AxConsoleTab->PrintError(message); }
