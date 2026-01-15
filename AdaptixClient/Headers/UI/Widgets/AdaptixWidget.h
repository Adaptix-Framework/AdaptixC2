#ifndef ADAPTIXCLIENT_ADAPTIXWIDGET_H
#define ADAPTIXCLIENT_ADAPTIXWIDGET_H

#include <Agent/Commander.h>
#include <main.h>

#include <kddockwidgets/qtwidgets/views/DockWidget.h>
#include <kddockwidgets/qtwidgets/views/MainWindow.h>

#include <QJSValue>

class Task;
class Agent;
class LastTickWorker;
class WebSocketWorker;
class SessionsTableWidget;
class SessionsGraph;
class AxConsoleWidget;
class LogsWidget;
class ChatWidget;
class ListenersWidget;
class DownloadsWidget;
class ScreenshotsWidget;
class CredentialsWidget;
class TargetsWidget;
class TasksWidget;
class TunnelsWidget;
class TunnelEndpoint;
class DialogSyncPacket;
class AuthProfile;
class AxScriptManager;

typedef struct RegListenerConfig {
    QString name;
    QString protocol;
    QString type;
} RegListenerConfig;

typedef struct RegAgentConfig {
    QString        name;
    QString        listenerType;
    int            os;
    Commander*     commander;
    bool           valid;
} RegAgentConfig;

typedef struct AgentTypeInfo {
    bool        multiListeners;
    QStringList listenerTypes;
} AgentTypeInfo;

class AdaptixWidget : public QWidget
{
Q_OBJECT
    QGridLayout*    mainGridLayout    = nullptr;
    QHBoxLayout*    topHLayout        = nullptr;
    QPushButton*    listenersButton   = nullptr;
    QPushButton*    logsButton        = nullptr;
    QPushButton*    chatButton        = nullptr;
    QPushButton*    sessionsButton    = nullptr;
    QPushButton*    graphButton       = nullptr;
    QPushButton*    tasksButton       = nullptr;
    QPushButton*    targetsButton     = nullptr;
    QPushButton*    tunnelButton      = nullptr;
    QPushButton*    downloadsButton   = nullptr;
    QPushButton*    credsButton       = nullptr;
    QPushButton*    screensButton     = nullptr;
    QPushButton*    keysButton        = nullptr;
    QPushButton*    reconnectButton   = nullptr;
    QSpacerItem*    horizontalSpacer1 = nullptr;
    QFrame*         line_1            = nullptr;
    QFrame*         line_2            = nullptr;
    QFrame*         line_3            = nullptr;
    QFrame*         line_4            = nullptr;

    KDDockWidgets::QtWidgets::MainWindow* mainDockWidget;
    KDDockWidgets::QtWidgets::DockWidget* dockTop;
    KDDockWidgets::QtWidgets::DockWidget* dockBottom;

    bool              synchronized     = false;
    bool              sync             = false;
    AuthProfile*      profile          = nullptr;
    DialogSyncPacket* dialogSyncPacket = nullptr;

    void createUI();

    static bool isValidSyncPacket(QJsonObject jsonObj);
    void processSyncPacket(QJsonObject jsonObj);

    void setSyncUpdateUI(bool enabled);

public:
    QThread*         ChannelThread   = nullptr;
    WebSocketWorker* ChannelWsWorker = nullptr;
    QThread*         TickThread      = nullptr;
    LastTickWorker*  TickWorker      = nullptr;

    AxScriptManager* ScriptManager = nullptr;

    AxConsoleWidget*     AxConsoleDock     = nullptr;
    LogsWidget*          LogsDock          = nullptr;
    ChatWidget*          ChatDock          = nullptr;
    ListenersWidget*     ListenersDock     = nullptr;
    SessionsTableWidget* SessionsTableDock = nullptr;
    SessionsGraph*       SessionsGraphDock = nullptr;
    TunnelsWidget*       TunnelsDock       = nullptr;
    DownloadsWidget*     DownloadsDock     = nullptr;
    ScreenshotsWidget*   ScreenshotsDock   = nullptr;
    CredentialsWidget*   CredentialsDock   = nullptr;
    TasksWidget*         TasksDock         = nullptr;
    TargetsWidget*       TargetsDock       = nullptr;

    QVector<RegListenerConfig>     RegisterListeners;
    QVector<RegAgentConfig>        RegisterAgents;
    QMap<QString, AgentTypeInfo>   AgentTypes;
    QVector<ListenerData>          Listeners;
    QVector<TunnelData>            Tunnels;
    QMap<QString, DownloadData>    Downloads;
    QMap<QString, ScreenData>      Screenshots;
    QVector<CredentialData>        Credentials;
    QVector<TargetData>            Targets;
    QMap<QString, PivotData>       Pivots;
    QMap<QString, TaskData>        TasksMap;
    QMap<QString, Agent*>          AgentsMap;
    QMap<QString, AxExecutor>      PostHooksJS;
    QMap<QString, AxExecutor>      PostHandlersJS;
    QMap<QString, TunnelEndpoint*> ClientTunnels;
    QStringList addresses;

    explicit AdaptixWidget(AuthProfile* authProfile, QThread* channelThread, WebSocketWorker* channelWsWorker);
    ~AdaptixWidget() override;

    AuthProfile* GetProfile() const;

    void PlaceDock(KDDockWidgets::QtWidgets::DockWidget* parentDock, KDDockWidgets::QtWidgets::DockWidget* dock) const;
    KDDockWidgets::QtWidgets::DockWidget* get_dockTop() {return dockTop;}
    KDDockWidgets::QtWidgets::DockWidget* get_dockBottom() {return dockBottom;}

    bool AddExtension(ExtensionFile* ext);
    void RemoveExtension(const ExtensionFile &ext);
    bool IsSynchronized();
    void Close();
    void ClearAdaptix();

    void RegisterListenerConfig(const QString &name, const QString &protocol, const QString &type, const QString &ax_script);
    void RegisterAgentConfig(const QString &agentName, const QString &ax_script, const QStringList &listenersconst, const bool &multiListeners);
    RegListenerConfig GetRegListener(const QString &listenerName);
    QList<QString>    GetAgentNames(const QString &listenerType) const;
    RegAgentConfig    GetRegAgent(const QString &agentName, const QString &listenerName, int os);
    AgentTypeInfo     GetAgentTypeInfo(const QString &agentName) const;
    QList<Commander*> GetCommanders(const QStringList &listeners, const QStringList &agents, const QList<int> &os) const;
    QList<Commander*> GetCommandersAll() const;

    void PostHookProcess(QJsonObject jsonHookObj);
    void PostHandlerProcess(const QString &handlerId, const TaskData &taskData);

    void LoadConsoleUI(const QString &AgentId);
    void LoadTasksOutput() const;
    void LoadFileBrowserUI(const QString &AgentId);
    void LoadProcessBrowserUI(const QString &AgentId);
    void LoadTerminalUI(const QString &AgentId);
    void LoadShellUI(const QString &AgentId);
    void ShowTunnelCreator(const QString &AgentId, bool socks4, bool socks5, bool lportfwd, bool rportfwd);

Q_SIGNALS:
    void SyncedSignal();
    void SyncedOnReloadSignal(QString project);
    void LoadGlobalScriptSignal(QString path);
    void UnloadGlobalScriptSignal(QString path);

    void eventNewAgent(QString agentId);
    void eventFileBrowserDisks(QString agentId);
    void eventFileBrowserList(QString agentId, QString path);
    void eventFileBrowserUpload(QString agentId, QString path, QString localFilename);
    void eventProcessBrowserList(QString agentId);

public Q_SLOTS:
    void ChannelClose() const;
    void DataHandler(const QByteArray& data);

    void OnWebSocketConnected();
    void OnSynced();
    void SetSessionsTableUI() const;
    void SetGraphUI() const;
    void SetTasksUI() const;
    void LoadAxConsoleUI() const;
    void LoadLogsUI() const;
    void LoadChatUI() const;
    void LoadListenersUI() const;
    void LoadTunnelsUI() const;
    void LoadDownloadsUI() const;
    void LoadScreenshotsUI() const;
    void LoadCredentialsUI() const;
    void LoadTargetsUI() const;
    void OnReconnect();
};

#endif