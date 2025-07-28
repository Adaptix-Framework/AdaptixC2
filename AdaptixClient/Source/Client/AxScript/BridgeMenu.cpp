#include <Client/AxScript/BridgeMenu.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/AxElementWrappers.h>

BridgeMenu::BridgeMenu(AxScriptEngine* scriptEngine, QObject* parent) : QObject(parent), scriptEngine(scriptEngine), widget(new QWidget()) {}

BridgeMenu::~BridgeMenu() { delete widget; }

void BridgeMenu::reg(const QString &type, AbstractAxMenuItem *item, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners)
{
    QSet<QString> list_agents;
    QSet<QString> list_os;
    QSet<QString> list_listeners;

    if (agents.isUndefined() || agents.isNull() || !agents.isArray() || agents.property("length").toInt() == 0)
        return;

    for (int i = 0; i < agents.property("length").toInt(); ++i) {
        QJSValue val = agents.property(i);
        list_agents.insert(val.toString());
    }

    if (!os.isUndefined() && !os.isNull() && os.isArray()) {
        for (int i = 0; i < os.property("length").toInt(); ++i) {
            QJSValue val = os.property(i);
            list_os << val.toString();
        }
    }

    if (!listeners.isUndefined() && !listeners.isNull() && listeners.isArray()) {
        for (int i = 0; i < listeners.property("length").toInt(); ++i) {
            QJSValue val = listeners.property(i);
            list_listeners << val.toString();
        }
    }

    this->scriptEngine->registerMenu(type, item, list_agents, list_os, list_listeners);
}

QList<AbstractAxMenuItem*> BridgeMenu::items() const { return menuItems; }

void BridgeMenu::clear()
{
    qDeleteAll(menuItems);
    menuItems.clear();
}

AxActionWrapper* BridgeMenu::create_action(const QString& text, const QJSValue& handler)
{
    auto* action = new AxActionWrapper(text, handler, scriptEngine->engine(), this);
    menuItems.append(action);
    return action;
}

AxSeparatorWrapper* BridgeMenu::create_separator()
{
    auto* sep = new AxSeparatorWrapper(this);
    menuItems.append(sep);
    return sep;
}

AxMenuWrapper* BridgeMenu::create_menu(const QString& title)
{
    auto* menu = new AxMenuWrapper(title, this);
    menuItems.append(menu);
    return menu;
}

void BridgeMenu::add_session_main(AbstractAxMenuItem *item, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners) {
    this->reg("SessionMain", item, agents, os, listeners);
}

void BridgeMenu::add_session_agent(AbstractAxMenuItem *item, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners) {
    this->reg("SessionAgent", item, agents, os, listeners);
}

void BridgeMenu::add_session_browser(AbstractAxMenuItem* item, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners){
    this->reg("SessionBrowser", item, agents, os, listeners);
}

void BridgeMenu::add_session_access(AbstractAxMenuItem* item, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners){
    this->reg("SessionAccess", item, agents, os, listeners);
}

void BridgeMenu::add_filebrowser(AbstractAxMenuItem *item, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners){
    this->reg("FileBrowser", item, agents, os, listeners);
}

void BridgeMenu::add_processbrowser(AbstractAxMenuItem *item, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners) {
    this->reg("ProcessBrowser", item, agents, os, listeners);
}

void BridgeMenu::add_downloads_running(AbstractAxMenuItem *item, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners){
    this->reg("DownloadRunning", item, agents, os, listeners);
}

void BridgeMenu::add_downloads_finished(AbstractAxMenuItem *item, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners){
    this->reg("DownloadFinished", item, agents, os, listeners);
}

void BridgeMenu::add_tasks(AbstractAxMenuItem *item, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners){
    this->reg("Tasks", item, agents, os, listeners);
}

void BridgeMenu::add_tasks_job(AbstractAxMenuItem *item, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners) {
    this->reg("TasksJob", item, agents, os, listeners);
}