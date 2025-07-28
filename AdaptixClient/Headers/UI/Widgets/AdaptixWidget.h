#ifndef ADAPTIXCLIENT_ADAPTIXWIDGET_H
#define ADAPTIXCLIENT_ADAPTIXWIDGET_H

#include <Agent/Commander.h>
#include <main.h>
#include <QJSValue>

class Task;
class Agent;
class LastTickWorker;
class WebSocketWorker;
class SessionsTableWidget;
class SessionsGraph;
class AxConsoleWidget;
class LogsWidget;
class ListenersWidget;
class DownloadsWidget;
class ScreenshotsWidget;
class CredentialsWidget;
class TasksWidget;
class TunnelsWidget;
class TunnelEndpoint;
class DialogSyncPacket;
class AuthProfile;
class AxScriptManager;

typedef struct RegAgentConfig {
    QString        name;
    QString        listenerType;
    int            os;
    Commander*     commander;
    bool           valid;
} RegAgentConfig;



class AdaptixWidget : public QWidget
{
Q_OBJECT
    QGridLayout*    mainGridLayout    = nullptr;
    QHBoxLayout*    topHLayout        = nullptr;
    QPushButton*    listenersButton   = nullptr;
    QPushButton*    logsButton        = nullptr;
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
    QFrame*         line_1            = nullptr;
    QFrame*         line_2            = nullptr;
    QFrame*         line_3            = nullptr;
    QFrame*         line_4            = nullptr;
    QSplitter*      mainVSplitter     = nullptr;
    QTabWidget*     mainTabWidget     = nullptr;
    QSpacerItem*    horizontalSpacer1 = nullptr;
    QStackedWidget* mainStackedWidget = nullptr;

    bool              synchronized     = false;
    bool              sync             = false;
    AuthProfile*      profile          = nullptr;
    DialogSyncPacket* dialogSyncPacket = nullptr;

    void createUI();

    static bool isValidSyncPacket(QJsonObject jsonObj);
    void processSyncPacket(QJsonObject jsonObj);

public:
    QThread*         ChannelThread   = nullptr;
    WebSocketWorker* ChannelWsWorker = nullptr;
    QThread*         TickThread      = nullptr;
    LastTickWorker*  TickWorker      = nullptr;

    AxScriptManager* ScriptManager = nullptr;

    AxConsoleWidget*     AxConsoleTab      = nullptr;
    LogsWidget*          LogsTab           = nullptr;
    ListenersWidget*     ListenersTab      = nullptr;
    SessionsTableWidget* SessionsTablePage = nullptr;
    SessionsGraph*       SessionsGraphPage = nullptr;
    TunnelsWidget*       TunnelsTab        = nullptr;
    DownloadsWidget*     DownloadsTab      = nullptr;
    ScreenshotsWidget*   ScreenshotsTab    = nullptr;
    CredentialsWidget*   CredentialsTab    = nullptr;
    TasksWidget*         TasksTab          = nullptr;

    QVector<RegAgentConfig>        RegisterAgents;
    QVector<ListenerData>          Listeners;
    QVector<TunnelData>            Tunnels;
    QMap<QString, DownloadData>    Downloads;
    QMap<QString, ScreenData>      Screenshots;
    QVector<CredentialData>        Credentials;
    QMap<QString, PivotData>       Pivots;
    QVector<QString>               TasksVector;
    QMap<QString, Task*>           TasksMap;
    QVector<QString>               AgentsVector;
    QMap<QString, Agent*>          AgentsMap;
    QMap<QString, PostHook>        PostHooksJS;
    QMap<QString, TunnelEndpoint*> ClientTunnels;
    QStringList addresses;

    explicit AdaptixWidget(AuthProfile* authProfile, QThread* channelThread, WebSocketWorker* channelWsWorker);
    ~AdaptixWidget() override;

    AuthProfile* GetProfile() const;

    void AddTab(QWidget* tab, const QString &title, const QString &icon = "" ) const;
    void RemoveTab(int index) const;
    bool AddExtension(ExtensionFile* ext);
    void RemoveExtension(const ExtensionFile &ext);
    void Close();
    void ClearAdaptix();

    void RegisterListenerConfig(const QString &fn, const QString &ax_script);
    void RegisterAgentConfig(const QString &agentName, const QString &ax_script, const QStringList &listeners);
    QList<QString> GetAgentNames(const QString &listenerType) const;
    RegAgentConfig GetRegAgent(const QString &agentName, const QString &listenerName, int os);
    QList<Commander*> GetCommanders(const QStringList &listeners, const QStringList &agents, const QList<int> &os) const;
    QList<Commander*> GetCommandersAll() const;

    void PostHookProcess(QJsonObject jsonHookObj);

    void LoadConsoleUI(const QString &AgentId);
    void LoadTasksOutput() const;
    void LoadFileBrowserUI(const QString &AgentId);
    void LoadProcessBrowserUI(const QString &AgentId);
    void LoadTerminalUI(const QString &AgentId);
    void ShowTunnelCreator(const QString &AgentId, bool socks4, bool socks5, bool lportfwd, bool rportfwd);

signals:
    void SyncedSignal();
    void SyncedOnReloadSignal(QString project);
    void LoadGlobalScriptSignal(QString path);
    void UnloadGlobalScriptSignal(QString path);

    void eventFileBrowserDisks(QString agentId);
    void eventFileBrowserList(QString agentId, QString path);
    void eventFileBrowserUpload(QString agentId, QString path, QString localFilename);
    void eventProcessBrowserList(QString agentId);

public slots:
    void ChannelClose() const;
    void DataHandler(const QByteArray& data);

    void OnSynced();
    void SetSessionsTableUI() const;
    void SetGraphUI() const;
    void SetTasksUI() const;
    void LoadAxConsoleUI() const;
    void LoadLogsUI() const;
    void LoadListenersUI() const;
    void LoadTunnelsUI() const;
    void LoadDownloadsUI() const;
    void LoadScreenshotsUI() const;
    void LoadCredentialsUI() const;
    void OnReconnect();
};

#endif