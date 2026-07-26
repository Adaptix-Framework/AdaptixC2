#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/BridgeApp.h>
#include <Client/AxScript/BridgeForm.h>
#include <Client/AxScript/BridgeEvent.h>
#include <Client/AxScript/BridgeMenu.h>
#include <Client/AxScript/BridgeProcess.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <Client/Settings.h>
#include <MainAdaptix.h>
#include <main.h>

#include <QDir>
#include <QFileInfo>

namespace {

const SettingsData* liveSettings()
{
    if (GlobalClient && GlobalClient->settings)
        return &GlobalClient->settings->data;
    return nullptr;
}

QString expandTildePath(QString path)
{
    path = path.trimmed();
    if (path.isEmpty())
        return path;

    if (path == QLatin1Char('~'))
        return QDir::homePath();

    if (path.startsWith(QLatin1String("~/")) || path.startsWith(QLatin1String("~\\")))
        return QDir::home().filePath(path.mid(2));

    return path;
}

QString normalizePathKey(QString path)
{
    path = expandTildePath(std::move(path));
    path = QDir::cleanPath(path);
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));

#ifdef Q_OS_WIN
    path = path.toLower();
    while (path.size() > 3 && path.endsWith(QLatin1Char('/')))
        path.chop(1);
#else
    while (path.size() > 1 && path.endsWith(QLatin1Char('/')))
        path.chop(1);
#endif
    return path;
}

bool pathIsUnderRoot(const QString& candidate, const QString& root)
{
    const QString c = normalizePathKey(candidate);
    const QString r = normalizePathKey(root);
    if (c.isEmpty() || r.isEmpty())
        return false;
    if (c == r)
        return true;
    return c.startsWith(r + QLatin1Char('/'));
}

QString absoluteOrCanonical(const QString& path)
{
    const QFileInfo fi(path);
    if (fi.exists()) {
        const QString canon = fi.canonicalFilePath();
        if (!canon.isEmpty())
            return canon;
    }
    if (QDir::isAbsolutePath(path))
        return QDir::cleanPath(path);
    return QDir::cleanPath(fi.absoluteFilePath());
}

} // namespace

AxScriptEngine::AxScriptEngine(AxScriptManager* script_manager, const QString &name, QObject *parent, const AxScriptTrust trust) : QObject(parent), scriptManager(script_manager), trustLevel(trust)
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

    if (allowProcess()) {
        bridgeProcess = std::make_unique<BridgeProcess>(this, this);
        jsEngine->globalObject().setProperty("process", jsEngine->newQObject(bridgeProcess.get()));
    }

    if (script_manager) {
        connect(bridgeApp.get(),   &BridgeApp::consoleError,   script_manager, &AxScriptManager::consolePrintError);
        connect(bridgeApp.get(),   &BridgeApp::consoleMessage, script_manager, &AxScriptManager::consolePrintMessage);
    }
    connect(bridgeApp.get(),   &BridgeApp::engineError,   this, &AxScriptEngine::engineError);
    connect(bridgeForm.get(),  &BridgeForm::scriptError,  this, &AxScriptEngine::engineError);
    connect(bridgeEvent.get(), &BridgeEvent::scriptError, this, &AxScriptEngine::engineError);

    context.name = name;

    if (sandboxFs()) {
        QDir().mkpath(sandboxRoot());
    }
}

AxScriptEngine::AxScriptEngine(AxScriptManager* script_manager, const QString &name, QObject *parent, const bool serverMode) : AxScriptEngine(script_manager, name, parent, serverMode ? AxScriptTrust::Server : AxScriptTrust::Local)
{
}

AxScriptEngine::~AxScriptEngine()
{
    for (auto action : context.actions) {
        if (action && !action->parent()) delete action;
    }
    context.actions.clear();
    context.objects.clear();

    for (auto it = context.events.begin(); it != context.events.end(); ++it) {
        if (it.value().timer) {
            it.value().timer->stop();
            delete it.value().timer;
        }
    }
    context.events.clear();

    context.menus.clear();

    bridgeApp.reset();
    bridgeForm.reset();
    bridgeEvent.reset();
    bridgeMenu.reset();
    bridgeProcess.reset();
    jsEngine.reset();
}

QJSEngine* AxScriptEngine::engine() const { return jsEngine.get(); }

BridgeApp* AxScriptEngine::app() const { return bridgeApp.get(); }

BridgeForm* AxScriptEngine::form() const { return bridgeForm.get(); }

BridgeEvent* AxScriptEngine::event() const { return bridgeEvent.get(); }

BridgeMenu* AxScriptEngine::menu() const { return bridgeMenu.get(); }

BridgeProcess* AxScriptEngine::process() const { return bridgeProcess.get(); }

AxScriptManager* AxScriptEngine::manager() const { return this->scriptManager; }

void AxScriptEngine::setTrust(AxScriptTrust t) { trustLevel = t; }

void AxScriptEngine::setServerMode(bool enabled) { trustLevel = enabled ? AxScriptTrust::Server : AxScriptTrust::Local; }

void AxScriptEngine::setEnabled(bool enabled) { scriptEnabled = enabled; }

bool AxScriptEngine::isEnabled() const { return scriptEnabled; }

AxScriptPolicy AxScriptEngine::policy() const
{
    AxScriptPolicy p;
    p.fileRead = true;
    p.fileWrite = (trustLevel != AxScriptTrust::Server);
    p.process = (trustLevel == AxScriptTrust::CodeEditorAction);
    p.sandboxFs = true;

    const SettingsData* s = liveSettings();
    if (!s)
        return p;

    switch (trustLevel) {
    case AxScriptTrust::Server:           return s->ScriptServer;
    case AxScriptTrust::Local:            return s->ScriptLocal;
    case AxScriptTrust::CodeEditor:       return s->ScriptEditor;
    case AxScriptTrust::CodeEditorAction: return s->ScriptEditorAction;
    }
    return p;
}

bool AxScriptEngine::allowFileRead() const  { return policy().fileRead; }
bool AxScriptEngine::allowFileWrite() const { return policy().fileWrite; }
bool AxScriptEngine::sandboxFs() const      { return policy().sandboxFs; }

bool AxScriptEngine::allowProcess() const
{
    if (trustLevel != AxScriptTrust::CodeEditorAction)
        return false;
    return policy().process;
}

QString AxScriptEngine::sandboxRoot() const
{
    const SettingsData* s = liveSettings();
    QString root;
    if (s && !s->ScriptSandboxDir.trimmed().isEmpty())
        root = s->ScriptSandboxDir.trimmed();
    else
        root = QDir(QDir::homePath()).filePath(QStringLiteral(".adaptix/script_sandbox"));

    root = expandTildePath(root);
    return QDir::cleanPath(root);
}

bool AxScriptEngine::resolveFsPath(QString& path, const bool forWrite, QString* errorOut) const
{
    auto fail = [&](const QString& msg) {
        if (errorOut) *errorOut = msg;
        return false;
    };

    if (forWrite) {
        if (!allowFileWrite())
            return fail(QStringLiteral("file_write is not allowed for this script context"));
    } else {
        if (!allowFileRead())
            return fail(QStringLiteral("file_read is not allowed for this script context"));
    }

    path = expandTildePath(path);
    if (path.isEmpty())
        return fail(QStringLiteral("empty path"));

    if (!sandboxFs()) {
        if (QDir::isRelativePath(path))
            path = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        else
            path = QDir::cleanPath(path);
        return true;
    }

    const QString root = sandboxRoot();
    QDir().mkpath(root);
    const QString rootAbs = absoluteOrCanonical(root);

    if (QDir::isRelativePath(path)) {
        path = QDir(rootAbs).filePath(path);
        path = QDir::cleanPath(path);
    } else {
        path = QDir::cleanPath(path);
    }

    QStringList allowedRoots;
    allowedRoots << rootAbs;

    if (trustLevel == AxScriptTrust::Local && !context.name.isEmpty()) {
        const QFileInfo scriptFi(expandTildePath(context.name));
        if (scriptFi.isAbsolute() || QFile::exists(scriptFi.filePath())) {
            const QString scriptDir = scriptFi.absolutePath();
            if (!scriptDir.isEmpty())
                allowedRoots << absoluteOrCanonical(scriptDir);
        }
    }

    const QString checkPath = absoluteOrCanonical(path);

    for (const QString& r : allowedRoots) {
        if (pathIsUnderRoot(checkPath, r) || pathIsUnderRoot(path, r)) {
            path = QDir::cleanPath(path);
            return true;
        }
    }

    return fail(QStringLiteral("path is outside the script sandbox: %1").arg(path));
}

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
