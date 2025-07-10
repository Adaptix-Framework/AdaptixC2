#ifndef AXSCRIPTMANAGER_H
#define AXSCRIPTMANAGER_H

#include <QObject>
#include <QMenu>
#include <QJSValue>

class AxScriptEngine;
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

    AdaptixWidget* GetAdaptix() const;
    QMap<QString, Agent*> GetAgents() const;

    QStringList ListenerScriptList();
    void        ListenerScriptAdd(const QString &name, const QString &ax_script);
    QJSEngine*  ListenerScriptEngine(const QString &name);
    QStringList AgentScriptList();
    void        AgentScriptAdd(const QString &name, const QString &ax_script);
    QJSEngine*  AgentScriptEngine(const QString &name);

    ////

    void ScriptAdd(const QString &name, AxScriptEngine* script);
    void ScriptRemove(const QString &name);
    // void ExScriptRemove(const QString &name);

    QJSValue AgentScriptExecute(const QString &name, const QString &code);

    int AddMenuSession(QMenu* menu, const QString &menuType, QStringList agentIds) const;

    void emitAllEventTestClick();

public slots:
    void consolePrintMessage(const QString &message);
    void consolePrintError(const QString &message);
};

#endif //AXSCRIPTMANAGER_H
