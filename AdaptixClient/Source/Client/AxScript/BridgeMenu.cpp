#include <Client/AxScript/BridgeMenu.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/AxElementWrappers.h>

BridgeMenu::BridgeMenu(AxScriptEngine* scriptEngine, QObject* parent) : QObject(parent), scriptEngine(scriptEngine), widget(new QWidget()) {}

BridgeMenu::~BridgeMenu() { delete widget; }

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

void BridgeMenu::add_session_main(AbstractAxMenuItem *item) const
{
    this->scriptEngine->registerMenu("SessionMain", item);
}

void BridgeMenu::add_session_access(AbstractAxMenuItem *item) const
{
    this->scriptEngine->registerMenu("SessionAccess", item);
}

const QList<AbstractAxMenuItem*>& BridgeMenu::items() const { return menuItems; }

void BridgeMenu::clear()
{
    qDeleteAll(menuItems);
    menuItems.clear();
}