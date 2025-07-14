#ifndef AXSCRIPTMANAGER_H
#define AXSCRIPTMANAGER_H

#include <QObject>
#include <QMenu>
#include <QJSValue>
#include <Agent/Commander.h>

class AxScriptEngine;
class ExtensionFile;
class AdaptixWidget;
class Agent;

class AxScriptManager : public QObject {
Q_OBJECT
    AdaptixWidget*  mainWidget = nullptr;
    AxScriptEngine* mainScript = nullptr;
    QMap<QString, AxScriptEngine*> scripts;
    QMap<QString, AxScriptEngine*> listeners_scripts;
    QMap<QString, AxScriptEngine*> agents_scripts;

public:
    AxScriptManager(AdaptixWidget* main_widget, QObject *parent = nullptr);
    ~AxScriptManager() override;

    QJSEngine* MainScriptEngine();
    void ResetMain();
    void Clear();

    AdaptixWidget*        GetAdaptix() const;
    QMap<QString, Agent*> GetAgents() const;
    QJSEngine*            GetEngine(const QString &name);

    QStringList ListenerScriptList();
    void        ListenerScriptAdd(const QString &name, const QString &ax_script);
    QJSEngine*  ListenerScriptEngine(const QString &name);

    QStringList AgentScriptList();
    void        AgentScriptAdd(const QString &name, const QString &ax_script);
    QJSEngine*  AgentScriptEngine(const QString &name);
    QJSValue    AgentScriptExecute(const QString &name, const QString &code);

    QStringList ScriptList();
    bool        ScriptAdd(ExtensionFile* ext);
    void        ScriptRemove(const ExtensionFile &ext);

    void GlobalScriptLoad(const QString &path);
    void GlobalScriptUnload(const QString &path);

    void RegisterCommandsGroup(const CommandsGroup &group, const QStringList &listeners, const QStringList &agents, const QList<int> &os);
    int  AddMenuSession(QMenu* menu, const QString &menuType, QStringList agentIds) const;

    void emitAllEventTestClick();

public slots:
    void consolePrintMessage(const QString &message);
    void consolePrintError(const QString &message);
};

#endif
