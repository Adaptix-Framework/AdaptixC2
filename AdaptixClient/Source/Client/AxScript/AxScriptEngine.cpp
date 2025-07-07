#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/BridgeApp.h>
#include <Client/AxScript/BridgeForm.h>
#include <Client/AxScript/BridgeEvent.h>
#include <Client/AxScript/BridgeMenu.h>

// #include <Classes/MainWindow.h>
// #include <Classes/AxContainers/AxScriptConsole.h>

AxScriptEngine::AxScriptEngine(AxScriptManager *script_manager, const QString &name, QObject *parent) : QObject(parent), scriptManager(script_manager)
{
    jsEngine = std::make_unique<QJSEngine>();
    jsEngine->installExtensions(QJSEngine::ConsoleExtension);

    bridgeApp   = std::make_unique<BridgeApp>(this, this);
    bridgeForm  = std::make_unique<BridgeForm>(this, this);
    bridgeEvent = std::make_unique<BridgeEvent>(this, this);
    bridgeMenu  = std::make_unique<BridgeMenu>(this, this);

    jsEngine->globalObject().setProperty("ax",      jsEngine->newQObject(bridgeApp.get()));
    jsEngine->globalObject().setProperty("form",    jsEngine->newQObject(bridgeForm.get()));
    jsEngine->globalObject().setProperty("event",   jsEngine->newQObject(bridgeEvent.get()));
    jsEngine->globalObject().setProperty("menu",    jsEngine->newQObject(bridgeMenu.get()));
    // connect(bridgeEvent.get(), &BridgeEvent::consoleAppendError, main->scriptConsole, &AxScriptConsole::appendErrorOutput);
    // connect(bridgeForm.get(),  &BridgeForm::consoleAppendError,  main->scriptConsole, &AxScriptConsole::appendErrorOutput);
    // connect(bridgeApp.get(),   &BridgeApp::consoleAppendError,   main->scriptConsole, &AxScriptConsole::appendErrorOutput);
    // connect(bridgeApp.get(),   &BridgeApp::consoleAppend,        main->scriptConsole, &AxScriptConsole::appendOutput);

    context.name = name;
}

AxScriptEngine::~AxScriptEngine()
{
    for (auto action : context.actions) {
        if (action) delete action;
    }
    context.actions.clear();
/*
    for (auto obj : context.objects){
        if (obj) delete obj;
    }
*/
    context.objects.clear();

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

void AxScriptEngine::registerMenu(const QString &type, AbstractAxMenuItem *menu, const QSet<QString> &list_agents, const QSet<QString> &list_os, const QSet<QString> &list_listeners)
{
    QSet<int> os;
    if (list_os.contains("windows")) os.insert(1);
    if (list_os.contains("linux")) os.insert(2);
    if (list_os.contains("macos")) os.insert(3);

    AxMenuItem item = {menu, list_agents,  list_listeners, os};

    if (     type == "SessionMain")    context.menuSessionMain.append(item);
    else if (type == "SessionAgent")   context.menuSessionAgent.append(item);
    else if (type == "SessionBrowser") context.menuSessionBrowser.append(item);
    else if (type == "SessionAccess")  context.menuSessionAccess.append(item);
}

QList<AxMenuItem> AxScriptEngine::getMenuItems(const QString &type)
{
    if (     type == "SessionMain")    return context.menuSessionMain;
    else if (type == "SessionAgent")   return context.menuSessionAgent;
    else if (type == "SessionBrowser") return context.menuSessionBrowser;
    else if (type == "SessionAccess")  return context.menuSessionAccess;

    return QList<AxMenuItem>();
}

bool AxScriptEngine::execute(const QString &code)
{
    QJSValue result = jsEngine->evaluate(code, context.name);
    if (result.isError()) {
        QString error = QStringLiteral("%1\n  at line %2 in %3\n  stack: %4")
            .arg(result.toString())
            .arg(result.property("lineNumber").toInt())
            .arg(result.property("fileName").toString())
            .arg(result.property("stack").toString());

        // auto main = qobject_cast<MainWindow*>( mainWidget );
        // main->scriptConsole->appendErrorOutput(error);
        return false;
    }
    context.scriptObject = result;
    return true;
}
