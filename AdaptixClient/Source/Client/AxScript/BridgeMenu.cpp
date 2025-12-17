#include <Client/AxScript/BridgeMenu.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <Client/AxScript/AxScriptUtils.h>

BridgeMenu::BridgeMenu(AxScriptEngine* scriptEngine, QObject* parent) : QObject(parent), scriptEngine(scriptEngine) {}

BridgeMenu::~BridgeMenu() = default;

void BridgeMenu::reg(const QString &type, AbstractAxMenuItem *item, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners)
{
    if (!AxScriptUtils::isValidNonEmptyArray(agents))
        return;

    QSet<QString> list_agents    = AxScriptUtils::jsArrayToStringSet(agents);
    QSet<QString> list_os        = AxScriptUtils::jsArrayToStringSet(os);
    QSet<QString> list_listeners = AxScriptUtils::jsArrayToStringSet(listeners);

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

void BridgeMenu::add_targets(AbstractAxMenuItem *item, const QString &position)
{
    if (position == "top") {
        this->scriptEngine->registerMenu("TargetsTop", item, QSet<QString>(), QSet<QString>(), QSet<QString>());
    }
    else if (position == "bottom") {
        this->scriptEngine->registerMenu("TargetsBottom", item, QSet<QString>(), QSet<QString>(), QSet<QString>());
    }
    else {
        this->scriptEngine->registerMenu("TargetsCenter", item, QSet<QString>(), QSet<QString>(), QSet<QString>());
    }
}

void BridgeMenu::add_credentials(AbstractAxMenuItem *item) {
    this->scriptEngine->registerMenu("Creds", item, QSet<QString>(), QSet<QString>(), QSet<QString>());
}
