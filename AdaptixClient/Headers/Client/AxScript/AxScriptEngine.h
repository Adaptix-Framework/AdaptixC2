#ifndef AXSCRIPTENGINE_H
#define AXSCRIPTENGINE_H

#include <QAction>
#include <QJSValue>
#include <QJSEngine>
#include <QString>
#include <QObject>
#include <QWidget>
#include <main.h>

class BridgeApp;
class BridgeForm;
class BridgeEvent;
class BridgeMenu;
class BridgeProcess;
class AbstractAxMenuItem;
class AxScriptManager;

enum class AxScriptTrust {
    Server = 0,
    Local = 1,
    CodeEditor = 2,
    CodeEditorAction = 3
};

struct AxEvent {
    QJSValue      handler;
    QTimer*       timer;
    QString       event_id;
    QSet<QString> agents;
    QSet<QString> listeners;
    QSet<int>     os;
    QJSEngine*    jsEngine;
};

struct AxMenuItem {
    AbstractAxMenuItem* menu;
    QSet<QString> agents;
    QSet<QString> listeners;
    QSet<int>     os;
};

struct ScriptContext {
    QString         name;
    QJSValue        scriptObject;
    QList<QObject*> objects;
    QList<QAction*> actions;

    QHash<QString, AxEvent> events;
    QHash<QString, QList<AxMenuItem>> menus;
};

class AxScriptEngine : public QObject {
Q_OBJECT
    AxScriptManager* scriptManager;

    std::unique_ptr<QJSEngine>     jsEngine;
    std::unique_ptr<BridgeApp>     bridgeApp;
    std::unique_ptr<BridgeForm>    bridgeForm;
    std::unique_ptr<BridgeEvent>   bridgeEvent;
    std::unique_ptr<BridgeMenu>    bridgeMenu;
    std::unique_ptr<BridgeProcess> bridgeProcess;

    AxScriptTrust trustLevel = AxScriptTrust::Local;
    bool scriptEnabled = true;

public:
    ScriptContext context;

    explicit AxScriptEngine(AxScriptManager* script_manager, const QString &name = "", QObject *parent = nullptr, AxScriptTrust trust = AxScriptTrust::Local);
    explicit AxScriptEngine(AxScriptManager* script_manager, const QString &name, QObject *parent, bool serverMode);
    ~AxScriptEngine() override;

    QJSEngine*   engine() const;
    BridgeApp*   app() const;
    BridgeForm*  form() const;
    BridgeEvent* event() const;
    BridgeMenu*  menu() const;
    BridgeProcess* process() const;

    AxScriptManager* manager() const;

    AxScriptTrust trust() const { return trustLevel; }
    void setTrust(AxScriptTrust t);
    bool isServerMode() const { return trustLevel == AxScriptTrust::Server; }
    void setServerMode(bool enabled);

    void setEnabled(bool enabled);
    bool isEnabled() const;

    AxScriptPolicy policy() const;
    bool allowFileRead() const;
    bool allowFileWrite() const;
    bool allowProcess() const;
    bool sandboxFs() const;
    QString sandboxRoot() const;
    bool resolveFsPath(QString& path, bool forWrite, QString* errorOut = nullptr) const;

    void registerObject(QObject* obj);
    void registerAction(QAction* action);
    void registerEvent(const QString &type, const QJSValue &handler, QTimer* timer, const QSet<QString> &list_agents, const QSet<QString> &list_os, const QSet<QString> &list_listeners, const QString &id);
    void removeEvent(const QString &id);
    QStringList listEvent();
    void registerMenu(const QString &type, AbstractAxMenuItem* menu, const QSet<QString> &list_agents, const QSet<QString> &list_os, const QSet<QString> &list_listeners);
    bool execute(const QString &code);

    QList<AxEvent>    getEvents(const QString &type);
    QList<AxMenuItem> getMenuItems(const QString &type);

public Q_SLOTS:
    void engineError(const QString &message);
};

#endif
