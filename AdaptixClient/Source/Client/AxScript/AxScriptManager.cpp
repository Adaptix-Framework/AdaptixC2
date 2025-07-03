#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/BridgeEvent.h>
#include <UI/Widgets/AdaptixWidget.h>

AxScriptManager::AxScriptManager(AdaptixWidget* main_widget, QObject *parent): QObject(parent), mainWidget(main_widget) {
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

void AxScriptManager::ResetMain() {
    if (mainScript)
        delete mainScript;

    mainScript = new AxScriptEngine(this, "main", this);
}

QMap<QString, Agent*> AxScriptManager::getAgents() const { return mainWidget->AgentsMap; }


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

void AxScriptManager::AddMenuSessionMain(QMenu *menu, const QVariantList& context) const
{
    QList<AxScriptEngine*> list = this->agents_scripts.values() + this->scripts.values();
    list.append(this->mainScript);

    QList<AbstractAxMenuItem*> items;
    for (const auto script : list)
        items += script->getMenuItems("SessionMain");

    for (int i = 0; i < items.size(); ++i) {
        items[i]->setContext(context);

        if (auto* item1 = dynamic_cast<AxSeparatorWrapper*>(items[i])) {
            menu->addAction(item1->action());
        }
        else if (auto* item2 = dynamic_cast<AxActionWrapper*>(items[i])) {
            menu->addAction(item2->action());
        }
        else if (auto* item3 = dynamic_cast<AxMenuWrapper*>(items[i])) {
            menu->addMenu(item3->menu());
        }
    }
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

/////////////////////

