#include <Agent/Agent.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/BridgeEvent.h>
#include <UI/Widgets/AdaptixWidget.h>

AxScriptManager::AxScriptManager(AdaptixWidget* main_widget, QObject *parent): QObject(parent), mainWidget(main_widget)
{
    mainScript = new AxScriptEngine(this, "main", this);
}

AxScriptManager::~AxScriptManager() = default;

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
    if (mainScript)
        delete mainScript;

    mainScript = new AxScriptEngine(this, "main", this);
}

AdaptixWidget* AxScriptManager::GetAdaptix() const { return this->mainWidget; }

QMap<QString, Agent*> AxScriptManager::GetAgents() const { return mainWidget->AgentsMap; }


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

QJSEngine * AxScriptManager::AgentScriptEngine(const QString &name)
{
    if (!agents_scripts.contains(name)) return nullptr;
    return agents_scripts[name]->engine();
}


void AxScriptManager::ScriptSetMain(AxScriptEngine *script) { mainScript = script; } /// ToDo:

void AxScriptManager::ScriptAdd(const QString &name, AxScriptEngine* script)
{
    if (scripts.contains(name)) return;
    scripts[name] = script;
}

void AxScriptManager::ScriptRemove(const QString &name)
{
    if (!scripts.contains(name)) return;
    auto scriptEngine = scripts.take(name);
    delete scriptEngine;
}


// void AxScriptManager::ExScriptRemove(const QString &name)
// {
//     if (!ex_scripts.contains(name))
//         return;
//
//     auto scriptEngine = ex_scripts.take(name);
//
//     delete scriptEngine;
// }



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

/// MENU

int AxScriptManager::AddMenuSession(QMenu *menu, const QString &menuType, QStringList agentIds) const
{
    QVariantList  context;
    QSet<QString> agentTypes;
    QSet<QString> listenerTypes;
    QSet<int>     osTypes;
    for (auto agent_id: agentIds) {
        if (mainWidget->AgentsMap.contains(agent_id)) {
            agentTypes.insert(mainWidget->AgentsMap[agent_id]->data.Name);
            osTypes.insert(mainWidget->AgentsMap[agent_id]->data.Os);
            listenerTypes.insert(mainWidget->AgentsMap[agent_id]->listenerType);
            context << agent_id;
        }
    }

    QList<AxScriptEngine*> list = this->agents_scripts.values() + this->scripts.values();
    list.append(this->mainScript);

    QList<AxMenuItem> items;
    for (const auto script : list)
        items += script->getMenuItems(menuType);

    int count = 0;
    for (int i = 0; i < items.size(); ++i) {
        AxMenuItem item = items[i];

        if ( !item.agents.contains(agentTypes) )
            continue;
        if (item.os.size() > 0 && !item.os.contains(osTypes))
            continue;
        if (item.listenerts.size() > 0 && !item.listenerts.contains(listenerTypes))
            continue;

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

void AxScriptManager::emitAllEventTestClick()
{
    if (mainScript && mainScript->event())
        mainScript->event()->emitEventTestClick();

    for (const auto& module : agents_scripts) { /// ?
        if (module->event())
            module->event()->emitEventTestClick();
    }

    for (const auto& module : scripts) {
        if (module->event())
            module->event()->emitEventTestClick();
    }
}
