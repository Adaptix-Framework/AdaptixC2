#ifndef AXSCRIPTMANAGER_H
#define AXSCRIPTMANAGER_H

#include <QObject>
#include <QMenu>
#include <QJSValue>
#include <Agent/Commander.h>
#include <main.h>

struct ExtensionFile;
struct AxMenuItem;
struct AxEvent;
class  AxScriptEngine;
class  AxScriptWorker;
class  AxUiFactory;
class  AdaptixWidget;
class  Agent;

struct DataMenuFileBrowser {
    QString agentId;
    QString path;
    QString name;
    QString type;
};

struct DataMenuProcessBrowser {
    QString agentId;
    QString pid;
    QString ppid;
    QString arch;
    QString session_id;
    QString context;
    QString process;
};

struct DataMenuDownload {
    QString agentId;
    QString fileId;
    QString path;
    QString state;
};

class AxScriptManager : public QObject {
Q_OBJECT
    AdaptixWidget*  adaptixWidget = nullptr;
    AxScriptEngine* mainScript    = nullptr;
    AxUiFactory*    uiFactory     = nullptr;
    QMap<QString, AxScriptEngine*> scripts;
    QMap<QString, AxScriptEngine*> listeners_scripts;
    QMap<QString, AxScriptEngine*> agents_scripts;

public:
    AxScriptManager(AdaptixWidget* main_widget, QObject *parent = nullptr);
    ~AxScriptManager() override;

    QJSEngine* MainScriptEngine();
    AxUiFactory* GetUiFactory() const;
    void ResetMain();
    void Clear();

    QJSEngine*                  GetEngine(const QString &name);
    AdaptixWidget*              GetAdaptix() const;
    QMap<QString, Agent*>       GetAgents() const;
    QVector<CredentialData>     GetCredentials() const;
    QMap<QString, DownloadData> GetDownloads() const;
    QMap<QString, ScreenData>   GetScreenshots() const;
    QVector<TargetData>         GetTargets() const;
    QVector<TunnelData>         GetTunnels() const;
    QStringList                 GetInterfaces() const;

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
    void GlobalScriptLoadAsync(const QString &path);
    
    void ExecuteAsync(const QString& code, const QString& name = "async");
    void ExecuteSmart(const QString& code, const QString& name = "smart");
    
    static bool containsUiCalls(const QString& code);

    void        RegisterCommandsGroup(const CommandsGroup &group, const QStringList &listeners, const QStringList &agents, const QList<int> &os);
    void        EventRemove(const QString &event_id);
    QStringList EventList();
    QList<AxMenuItem> FilterMenuItems(const QStringList &agentIds, const QString &menuType);
    QList<AxEvent>    FilterEvents(const QString &agentId, const QString &eventType);

    QList<AxScriptEngine*> getAllEngines() const;
    void safeCallHandler(const AxEvent& event, const QJSValueList& args = QJSValueList());
    int  addMenuItemsToMenu(QMenu* menu, const QList<AxMenuItem>& items, const QVariantList& context);

    void AppAgentHide(const QStringList &agents);
    void AppAgentRemove(const QStringList &agents);
    void AppAgentSetColor(const QStringList &agents, const QString &background, const QString &foreground, const bool reset);
    void AppAgentSetImpersonate(const QString &id, const QString &impersonate, const bool elevated);
    void AppAgentSetMark(const QStringList &agents, const QString &mark);
    void AppAgentSetTag(const QStringList &agents, const QString &tag);

    int AddMenuSession(QMenu* menu, const QString &menuType, QStringList agentIds);
    int AddMenuFileBrowser(QMenu* menu, QVector<DataMenuFileBrowser> files);
    int AddMenuProcessBrowser(QMenu* menu, QVector<DataMenuProcessBrowser> processes);
    int AddMenuDownload(QMenu* menu, const QString &menuType, QVector<DataMenuDownload> files);
    int AddMenuTask(QMenu* menu, const QString &menuType, const QStringList &tasks);
    int AddMenuTargets(QMenu* menu, const QString &menuType, const QStringList &targets);
    int AddMenuCreds(QMenu* menu, const QString &menuType, const QStringList &creds);

public Q_SLOTS:
    void consolePrintMessage(const QString &message);
    void consolePrintError(const QString &message);

    void emitNewAgent(const QString &agentId);
    void emitFileBrowserDisks(const QString &agentId);
    void emitFileBrowserList(const QString &agentId, const QString &path);
    void emitFileBrowserUpload(const QString &agentId, const QString &path, const QString &localFilename);
    void emitProcessBrowserList(const QString &agentId);
    void emitReadyClient();
    void emitDisconnectClient();
};

#endif
