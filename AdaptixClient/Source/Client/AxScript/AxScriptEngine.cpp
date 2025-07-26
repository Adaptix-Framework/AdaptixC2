#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/BridgeApp.h>
#include <Client/AxScript/BridgeForm.h>
#include <Client/AxScript/BridgeEvent.h>
#include <Client/AxScript/BridgeMenu.h>

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

    connect(bridgeApp.get(),   &BridgeApp::consoleError,   script_manager, &AxScriptManager::consolePrintError);
    connect(bridgeApp.get(),   &BridgeApp::consoleMessage, script_manager, &AxScriptManager::consolePrintMessage);
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

void AxScriptEngine::registerEvent(const QString &type, const QJSValue &handler, const QSet<QString> &list_agents, const QSet<QString> &list_os, const QSet<QString> &list_listeners, const QString &id)
{
    QSet<int> os;
    if (list_os.contains("windows")) os.insert(1);
    if (list_os.contains("linux")) os.insert(2);
    if (list_os.contains("macos")) os.insert(3);

    AxEvent event = {handler, id, list_agents,  list_listeners, os, jsEngine.get()};

    if (     type == "FileBroserDisks")    context.eventFileBroserDisks.append(event);
    else if (type == "FileBroserList")     context.eventFileBroserList.append(event);
    else if (type == "FileBroserUpload")   context.eventFileBroserUpload.append(event);
    else if (type == "ProcessBrowserList") context.eventProcessBrowserList.append(event);
}

QList<AxEvent> AxScriptEngine::getEvents(const QString &type)
{
    if (     type == "FileBroserDisks")    return context.eventFileBroserDisks;
    else if (type == "FileBroserList")     return context.eventFileBroserList;
    else if (type == "FileBroserUpload")   return context.eventFileBroserUpload;
    else if (type == "ProcessBrowserList") return context.eventProcessBrowserList;

    return QList<AxEvent>();
}

void AxScriptEngine::removeEvent(const QString &id)
{
    for (int i=0; i< context.eventFileBroserDisks.size(); i++) {
        if (id == context.eventFileBroserDisks[i].event_id) {
            context.eventFileBroserDisks.removeAt(i);
            i--;
        }
    }
}


/////

void AxScriptEngine::registerMenu(const QString &type, AbstractAxMenuItem *menu, const QSet<QString> &list_agents, const QSet<QString> &list_os, const QSet<QString> &list_listeners)
{
    QSet<int> os;
    if (list_os.contains("windows")) os.insert(1);
    if (list_os.contains("linux")) os.insert(2);
    if (list_os.contains("macos")) os.insert(3);

    AxMenuItem item = {menu, list_agents,  list_listeners, os};

    if (     type == "SessionMain")      context.menuSessionMain.append(item);
    else if (type == "SessionAgent")     context.menuSessionAgent.append(item);
    else if (type == "SessionBrowser")   context.menuSessionBrowser.append(item);
    else if (type == "SessionAccess")    context.menuSessionAccess.append(item);
    else if (type == "FileBrowser")      context.menuFileBrowser.append(item);
    else if (type == "ProcessBrowser")   context.menuProcessBrowser.append(item);
    else if (type == "DownloadRunning")  context.menuDownloadRunning.append(item);
    else if (type == "DownloadFinished") context.menuDownloadFinished.append(item);
    else if (type == "Tasks")            context.menuTasks.append(item);
    else if (type == "TasksJob")         context.menuTasksJob.append(item);
}

QList<AxMenuItem> AxScriptEngine::getMenuItems(const QString &type)
{
    if (     type == "SessionMain")      return context.menuSessionMain;
    else if (type == "SessionAgent")     return context.menuSessionAgent;
    else if (type == "SessionBrowser")   return context.menuSessionBrowser;
    else if (type == "SessionAccess")    return context.menuSessionAccess;
    else if (type == "FileBrowser")      return context.menuFileBrowser;
    else if (type == "ProcessBrowser")   return context.menuProcessBrowser;
    else if (type == "DownloadRunning")  return context.menuDownloadRunning;
    else if (type == "DownloadFinished") return context.menuDownloadFinished;
    else if (type == "Tasks")            return context.menuTasks;
    else if (type == "TasksJob")         return context.menuTasksJob;

    return QList<AxMenuItem>();
}

void AxScriptEngine::engineError(const QString &message) { engine()->throwError(QJSValue::TypeError, message); }

bool AxScriptEngine::execute(const QString &code)
{
    QJSValue result = jsEngine->evaluate(code, context.name);
    context.scriptObject = result;
    if (result.isError()) {
        QString error = QStringLiteral("%1\n    at line %2 in %3\n    stack: %4\n").arg(result.toString()).arg(result.property("lineNumber").toInt()).arg(context.name).arg(result.property("stack").toString());
        scriptManager->consolePrintError(error);
        return false;
    }
    return true;
}
