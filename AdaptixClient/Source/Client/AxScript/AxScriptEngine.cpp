#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/BridgeApp.h>
#include <Client/AxScript/BridgeForm.h>
#include <Client/AxScript/BridgeEvent.h>
#include <Client/AxScript/BridgeMenu.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <UI/MainUI.h>
#include <MainAdaptix.h>
#include <main.h>

AxScriptEngine::AxScriptEngine(AxScriptManager* script_manager, const QString &name, QObject *parent) : QObject(parent), scriptManager(script_manager)
{
    jsEngine = std::make_unique<QJSEngine>();
    jsEngine->installExtensions(QJSEngine::ConsoleExtension);

    bridgeApp   = std::make_unique<BridgeApp>(this, this);
    bridgeForm  = std::make_unique<BridgeForm>(this, this);
    bridgeEvent = std::make_unique<BridgeEvent>(this, this);
    bridgeMenu  = std::make_unique<BridgeMenu>(this, this);

    jsEngine->globalObject().setProperty("ax",    jsEngine->newQObject(bridgeApp.get()));
    jsEngine->globalObject().setProperty("form",  jsEngine->newQObject(bridgeForm.get()));
    jsEngine->globalObject().setProperty("event", jsEngine->newQObject(bridgeEvent.get()));
    jsEngine->globalObject().setProperty("menu",  jsEngine->newQObject(bridgeMenu.get()));

    if (script_manager) {
        connect(bridgeApp.get(),   &BridgeApp::consoleError,   script_manager, &AxScriptManager::consolePrintError);
        connect(bridgeApp.get(),   &BridgeApp::consoleMessage, script_manager, &AxScriptManager::consolePrintMessage);
    }
    connect(bridgeApp.get(),   &BridgeApp::engineError,   this, &AxScriptEngine::engineError);
    connect(bridgeForm.get(),  &BridgeForm::scriptError,  this, &AxScriptEngine::engineError);
    connect(bridgeEvent.get(), &BridgeEvent::scriptError, this, &AxScriptEngine::engineError);

    context.name = name;
}

AxScriptEngine::~AxScriptEngine()
{
    for (auto action : context.actions) {
        if (action) delete action;
    }
    context.actions.clear();
    context.objects.clear();

    for (auto it = context.events.begin(); it != context.events.end(); ++it) {
        if (it.value().timer) {
            it.value().timer->stop();
            it.value().timer->deleteLater();
        }
    }
    context.events.clear();

    // Remove main menu items
    if (GlobalClient && GlobalClient->mainUI) {
        for (const QString& type : {"MainMenu", "MainProjects", "MainAxScript", "MainSettings"}) {
            for (const auto& item : context.menus.value(type)) {
                if (auto* act = dynamic_cast<AxActionWrapper*>(item.menu)) {
                    if (type == "MainProjects")
                        GlobalClient->mainUI->getMenuProject()->removeAction(act->action());
                    else if (type == "MainAxScript")
                        GlobalClient->mainUI->getMenuAxScript()->removeAction(act->action());
                    else if (type == "MainSettings")
                        GlobalClient->mainUI->getMenuSettings()->removeAction(act->action());
                }
                else if (auto* sep = dynamic_cast<AxSeparatorWrapper*>(item.menu)) {
                    if (type == "MainProjects")
                        GlobalClient->mainUI->getMenuProject()->removeAction(sep->action());
                    else if (type == "MainAxScript")
                        GlobalClient->mainUI->getMenuAxScript()->removeAction(sep->action());
                    else if (type == "MainSettings")
                        GlobalClient->mainUI->getMenuSettings()->removeAction(sep->action());
                }
                else if (auto* sub = dynamic_cast<AxMenuWrapper*>(item.menu)) {
                    if (type == "MainMenu")
                        GlobalClient->mainUI->menuBar()->removeAction(sub->menu()->menuAction());
                    else if (type == "MainProjects")
                        GlobalClient->mainUI->getMenuProject()->removeAction(sub->menu()->menuAction());
                    else if (type == "MainAxScript")
                        GlobalClient->mainUI->getMenuAxScript()->removeAction(sub->menu()->menuAction());
                    else if (type == "MainSettings")
                        GlobalClient->mainUI->getMenuSettings()->removeAction(sub->menu()->menuAction());
                }
            }
        }
    }
    context.menus.clear();

    bridgeApp.reset();
    bridgeForm.reset();
    bridgeEvent.reset();
    bridgeMenu.reset();
    jsEngine.reset();
}

QJSEngine* AxScriptEngine::engine() const { return jsEngine.get(); }

BridgeApp* AxScriptEngine::app() const { return bridgeApp.get(); }

BridgeForm* AxScriptEngine::form() const { return bridgeForm.get(); }

BridgeEvent* AxScriptEngine::event() const { return bridgeEvent.get(); }

BridgeMenu* AxScriptEngine::menu() const { return bridgeMenu.get(); }

AxScriptManager* AxScriptEngine::manager() const { return this->scriptManager; }

void AxScriptEngine::registerObject(QObject *obj) { context.objects.append(obj); }

void AxScriptEngine::registerAction(QAction *action) { context.actions.append(action); }

/////

void AxScriptEngine::registerEvent(const QString &type, const QJSValue &handler, QTimer* timer, const QSet<QString> &list_agents, const QSet<QString> &list_os, const QSet<QString> &list_listeners, const QString &id)
{
    QSet<int> os;
    if (list_os.contains("windows")) os.insert(1);
    if (list_os.contains("linux")) os.insert(2);
    if (list_os.contains("macos")) os.insert(3);

    QString eventKey = id.isEmpty() ? type + "_" + GenerateRandomString(8, "hex") : id;
    AxEvent event = {handler, timer, eventKey, list_agents, list_listeners, os, jsEngine.get()};
    event.event_id = eventKey;

    context.events.insert(eventKey, event);
}

QList<AxEvent> AxScriptEngine::getEvents(const QString &type)
{
    QList<AxEvent> result;
    for (auto it = context.events.constBegin(); it != context.events.constEnd(); ++it) {
        const QString& key = it.key();
        if (key.startsWith(type + "_") || key == type ||
            (type == "timer" && (key.startsWith("interval_") || key.startsWith("timeout_")))) {
            result.append(it.value());
        }
    }
    return result;
}

void AxScriptEngine::removeEvent(const QString &id)
{
    if (context.events.contains(id)) {
        AxEvent event = context.events.take(id);
        if (event.timer) {
            event.timer->stop();
            event.timer->deleteLater();
        }
    }
}

QStringList AxScriptEngine::listEvent()
{
    QStringList list;
    for (auto it = context.events.constBegin(); it != context.events.constEnd(); ++it) {
        if (!it.key().isEmpty())
            list.append(it.key());
    }
    return list;
}

/////

void AxScriptEngine::registerMenu(const QString &type, AbstractAxMenuItem *menu, const QSet<QString> &list_agents, const QSet<QString> &list_os, const QSet<QString> &list_listeners)
{
    QSet<int> os;
    if (list_os.contains("windows")) os.insert(1);
    if (list_os.contains("linux"))   os.insert(2);
    if (list_os.contains("macos"))   os.insert(3);

    AxMenuItem item = {menu, list_agents, list_listeners, os};
    context.menus[type].append(item);

    if (type.startsWith("Main") && GlobalClient && GlobalClient->mainUI) {
        menu->setContext(QVariantList());

        QMenu* targetMenu = nullptr;
        if (type == "MainProjects")
            targetMenu = GlobalClient->mainUI->getMenuProject();
        else if (type == "MainAxScript")
            targetMenu = GlobalClient->mainUI->getMenuAxScript();
        else if (type == "MainSettings")
            targetMenu = GlobalClient->mainUI->getMenuSettings();

        if (targetMenu) {
            if (auto* sep = dynamic_cast<AxSeparatorWrapper*>(menu))
                targetMenu->addAction(sep->action());
            else if (auto* act = dynamic_cast<AxActionWrapper*>(menu))
                targetMenu->addAction(act->action());
            else if (auto* sub = dynamic_cast<AxMenuWrapper*>(menu))
                targetMenu->addMenu(sub->menu());
        } else if (type == "MainMenu") {
            if (auto* sub = dynamic_cast<AxMenuWrapper*>(menu))
                GlobalClient->mainUI->menuBar()->addMenu(sub->menu());
        }
    }
}

QList<AxMenuItem> AxScriptEngine::getMenuItems(const QString &type)
{
    return context.menus.value(type);
}

void AxScriptEngine::engineError(const QString &message) { engine()->throwError(QJSValue::TypeError, message); }

bool AxScriptEngine::execute(const QString &code)
{
    QJSValue result = jsEngine->evaluate(code, context.name);
    context.scriptObject = result;
    if (result.isError()) {
        QString error = QStringLiteral("%1\n    at line %2 in %3\n    stack: %4\n")
            .arg(result.toString())
            .arg(result.property("lineNumber").toInt())
            .arg(context.name)
            .arg(result.property("stack").toString());
        if (scriptManager)
            scriptManager->consolePrintError(error);
        return false;
    }
    return true;
}
