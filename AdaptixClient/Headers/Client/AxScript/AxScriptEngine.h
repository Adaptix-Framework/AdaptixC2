#ifndef AXSCRIPTENGINE_H
#define AXSCRIPTENGINE_H

#include <QAction>
#include <QJSValue>
#include <QJSEngine>
#include <QString>
#include <QObject>
#include <QWidget>

class BridgeApp;
class BridgeForm;
class BridgeEvent;
class BridgeMenu;
class AbstractAxMenuItem;
class AxScriptManager;

struct AxMenuItem {
    AbstractAxMenuItem* menu;
    QSet<QString> agents;
    QSet<QString> listenerts;
    QSet<int>     os;
};

struct ScriptContext {
    QString         name;
    QJSValue        scriptObject;
    QList<QObject*> objects;
    QList<QAction*> actions;

    QList<AxMenuItem> menuSessionMain;
    QList<AxMenuItem> menuSessionAgent;
    QList<AxMenuItem> menuSessionBrowser;
    QList<AxMenuItem> menuSessionAccess;
    QList<AxMenuItem> menuFileBrowser;
};

class AxScriptEngine : public QObject {
Q_OBJECT
    AxScriptManager* scriptManager;

    std::unique_ptr<QJSEngine>   jsEngine;
    std::unique_ptr<BridgeApp>   bridgeApp;
    std::unique_ptr<BridgeForm>  bridgeForm;
    std::unique_ptr<BridgeEvent> bridgeEvent;
    std::unique_ptr<BridgeMenu>  bridgeMenu;

public:
    ScriptContext context;

    explicit AxScriptEngine(AxScriptManager* script_manager, const QString &name = "", QObject *parent = nullptr);
    ~AxScriptEngine() override;

    QJSEngine*   engine() const;
    BridgeApp*   app() const;
    BridgeForm*  form() const;
    BridgeEvent* event() const;
    BridgeMenu*  menu() const;

    AxScriptManager* manager() const;

    void registerObject(QObject* obj);
    void registerAction(QAction* action);
    void registerMenu(const QString &type, AbstractAxMenuItem* menu, const QSet<QString> &list_agents, const QSet<QString> &list_os, const QSet<QString> &list_listeners);
    bool execute(const QString &code);

    QList<AxMenuItem> getMenuItems(const QString &type);

public slots:
    void engineError(const QString &message);
};

#endif
