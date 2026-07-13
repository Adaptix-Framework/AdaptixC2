#ifndef ADAPTIXCLIENT_ADAPTIXWIDGET_H
#define ADAPTIXCLIENT_ADAPTIXWIDGET_H

#include <Agent/Commander.h>
#include <main.h>

#include <kddockwidgets/qtwidgets/views/DockWidget.h>
#include <kddockwidgets/qtwidgets/views/MainWindow.h>

#include <oclero/qlementine/widgets/PopoverButton.hpp>
#include <oclero/qlementine/widgets/Popover.hpp>
#include <oclero/qlementine/widgets/NotificationBadge.hpp>

#include <QJSValue>
#include <QQueue>
#include <QReadWriteLock>
#include <QElapsedTimer>
#include <QListWidget>
#include <QDialog>
#include <functional>

class Task;
class Agent;
class LastTickWorker;
class WebSocketWorker;
class SessionsFeedWidget;
class SessionsGraph;
class ScriptsWidget;
class CodeEditorWidget;
class LogsWidget;
class ChatWidget;
class ListenersFeedWidget;
class FilesFeedWidget;
class ScreenshotsFeedWidget;
class CredentialsFeedWidget;
class TargetsFeedWidget;
class TasksFeedWidget;
class TunnelsFeedWidget;
class TunnelEndpoint;
class DialogSyncPacket;
class AuthProfile;
class AxScriptManager;
class ConnectionStatusWidget;
struct ServerScriptGroup;

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

struct ServerScriptInfo {
    QString name;
    QString description;
    bool    enabled;
};

class AdaptixWidget : public QWidget
{
Q_OBJECT
    QGridLayout*    mainGridLayout    = nullptr;
    QWidget*        toolbarWidget     = nullptr;
    QBoxLayout*     toolbarLayout     = nullptr;
    QFrame*         groupView         = nullptr;
    QFrame*         groupInfra        = nullptr;
    QObject*        m_groupPainter    = nullptr;
    QFrame*         groupData         = nullptr;
    QFrame*         groupDev          = nullptr;
    QPushButton*    listenersButton   = nullptr;
    QPushButton*    logsButton        = nullptr;
    QPushButton*    chatButton        = nullptr;
    oclero::qlementine::NotificationBadge* chatBadge = nullptr;
    QPushButton*    sessionsButton    = nullptr;
    QPushButton*    graphButton       = nullptr;
    QPushButton*    tasksButton       = nullptr;
    QPushButton*    targetsButton     = nullptr;
    QPushButton*    tunnelButton      = nullptr;
    QPushButton*    downloadsButton   = nullptr;
    QPushButton*    credsButton       = nullptr;
    QPushButton*    screensButton     = nullptr;
    QPushButton*    keysButton        = nullptr;
    QPushButton*    scriptManagerButton = nullptr;
    QPushButton*    codeEditorButton    = nullptr;
    QPushButton*    settingsButton      = nullptr;
    ConnectionStatusWidget* connStatusWidget = nullptr;
    oclero::qlementine::PopoverButton* extDocksButton = nullptr;

    oclero::qlementine::Popover* extDocksPopover = nullptr;
    QListWidget*    extDocksListWidget = nullptr;
    QLabel*         extDocksEmptyLabel = nullptr;

    KDDockWidgets::QtWidgets::MainWindow* mainDockWidget;
    KDDockWidgets::QtWidgets::DockWidget* dockTop;
    KDDockWidgets::QtWidgets::DockWidget* dockBottom;

    bool              synchronized     = false;
    bool              sync             = false;
    bool              syncFinishReceived = false;
    int               syncTotalBatches = 0;
    int               syncProcessingBatchIndex = 0;
    int               syncProcessingBatchTotal = 0;
    int               syncProcessingBatchProcessed = 0;
    QElapsedTimer     syncProcessingUiTimer;
    AuthProfile*      profile          = nullptr;
    DialogSyncPacket* dialogSyncPacket = nullptr;

    QQueue<QJsonObject> pendingPackets;
    QTimer*             pendingPacketsTimer = nullptr;

    QMultiMap<qint64, QJsonObject> deferredTaskPackets;
    QMultiMap<qint64, QJsonObject> deferredTransferPackets;

    void replayDeferredTaskPackets(qint64 taskId);
    void replayDeferredTransferPackets(qint64 fileId);

    void createUI();

    static bool isValidSyncPacket(QJsonObject jsonObj);
    void enqueueSyncPacket(const QJsonObject &jsonObj);
    void processPendingSyncPackets();
    void processSyncPacket(QJsonObject jsonObj);

    void finalizeSyncIfReady();

    void setSyncUpdateUI(bool enabled);
    void createButtons();
    void buildSegmentedGroups(bool vertical = false);
    void buildToolbarLayout(int position);
    void placeToolbarInGrid(QGridLayout* grid, int position);
    void applyThemeColorsToToolbar();

protected:
    void changeEvent(QEvent* event) override;

public:
    QThread*         ChannelThread   = nullptr;
    WebSocketWorker* ChannelWsWorker = nullptr;
    QThread*         TickThread      = nullptr;
    LastTickWorker*  TickWorker      = nullptr;

    AxScriptManager* ScriptManager = nullptr;

    ScriptsWidget*         ScriptsDock       = nullptr;
    CodeEditorWidget*      CodeEditorDock    = nullptr;
    LogsWidget*            LogsDock          = nullptr;
    ChatWidget*            ChatDock          = nullptr;
    ListenersFeedWidget*   ListenersDock     = nullptr;
    SessionsFeedWidget*    SessionsTableDock = nullptr;
    SessionsGraph*         SessionsGraphDock = nullptr;
    TunnelsFeedWidget*     TunnelsDock       = nullptr;
    FilesFeedWidget*       DownloadsDock     = nullptr;
    ScreenshotsFeedWidget* ScreenshotsDock   = nullptr;
    CredentialsFeedWidget* CredentialsDock   = nullptr;
    TasksFeedWidget*       TasksDock         = nullptr;
    TargetsFeedWidget*     TargetsDock       = nullptr;

    mutable bool CodeEditorDockPlaced = false;
    mutable bool TasksDockPlaced      = false;
    mutable bool TasksOutputDockPlaced = false;

    QVector<RegListenerConfig>       RegisterListeners;
    QVector<RegAgentConfig>          RegisterAgents;
    QMap<QString, AgentTypeInfo>     AgentTypes;
    QVector<ListenerData>            Listeners;
    QVector<TunnelData>              Tunnels;
    QMap<qint64, TransferData>       Downloads;
    QMap<qint64, TransferData>       Uploads;
    QMap<qint64, ScreenData>         Screenshots;
    QVector<CredentialData>          Credentials;
    QVector<TargetData>              Targets;
    QMap<QString, PivotData>         Pivots;
    QMap<qint64, TaskData>           TasksMap;
    QMap<qint64, Agent*>             AgentsMap;
    mutable QReadWriteLock           AgentsMapLock;
    mutable QReadWriteLock           TasksMapLock;
    mutable QReadWriteLock           CredentialsLock;
    mutable QReadWriteLock           DownloadsLock;
    mutable QReadWriteLock           UploadsLock;
    mutable QReadWriteLock           ScreenshotsLock;
    mutable QReadWriteLock           TargetsLock;
    mutable QReadWriteLock           TunnelsLock;
    QMap<QString, AxExecutor>        PostHooksJS;
    QMap<QString, AxExecutor>        PostHandlersJS;
    mutable QReadWriteLock           PostHooksLock;
    mutable QReadWriteLock           PostHandlersLock;
    QMap<qint64, TunnelEndpoint*>    ClientTunnels;
    QMap<qint64, QPair<qint64, int>> GraphTunnelMarks;
    QStringList addresses;

    struct ExtDockEntry {
        QString id;
        QString title;
        std::function<void()> showCallback;
    };
    QMap<QString, ExtDockEntry> extDocksMap;

    explicit AdaptixWidget(AuthProfile* authProfile, QThread* channelThread, WebSocketWorker* channelWsWorker);
    ~AdaptixWidget() override;

    AuthProfile* GetProfile() const;

    void rebuildToolbarLayout(int position);

    void PlaceDock(KDDockWidgets::QtWidgets::DockWidget* parentDock, KDDockWidgets::QtWidgets::DockWidget* dock) const;
    KDDockWidgets::QtWidgets::DockWidget* get_dockTop() {return dockTop;}
    KDDockWidgets::QtWidgets::DockWidget* get_dockBottom() {return dockBottom;}

    bool AddExtension(ExtensionFile* ext);
    void RemoveExtension(const ExtensionFile &ext);
    bool IsSynchronized();
    void Close();
    void ClearAdaptix();
    void ClearChatStream();
    void ChatUnreadIncrement();
    void ChatUnreadClear();
    void ClearConsoleStreams();
    void ClearNotificationsStream();

    void RegisterListenerConfig(const QString &name, const QString &protocol, const QString &type, const QString &ax_script);
    void RegisterAgentConfig(const QString &agentName, const QString &ax_script, const QStringList &listenersconst, const bool &multiListeners, const QJsonArray &groups);
    void RegisterServiceConfig(const QString &serviceName, const QString &ax_script);
    void ProcessAxScriptPacket(const QString &name, const QString &content, const QJsonArray &groups);
    void registerServerCommandGroups(const QString &scriptName, const QList<ServerScriptGroup> &groups, QJSEngine* engine);
    void EnableServerScript(const QString &name);
    void DisableServerScript(const QString &name);
    QList<ServerScriptInfo> GetServerScripts() const;
    RegListenerConfig GetRegListener(const QString &listenerName);
    QList<QString>    GetAgentNames(const QString &listenerType) const;
    RegAgentConfig    GetRegAgent(const QString &agentName, const QString &listenerName, int os);
    AgentTypeInfo     GetAgentTypeInfo(const QString &agentName) const;
    QList<Commander*> GetCommanders(const QStringList &listeners, const QStringList &agents, const QList<int> &os) const;
    QList<Commander*> GetCommandersAll() const;
    void              AddCommandsToCommanders(const CommandsGroup &group, const QStringList &listeners, const QStringList &agents, const QList<int> &os);

    void PostHookProcess(QJsonObject jsonHookObj);
    void PostHandlerProcess(const QString &handlerId, const TaskData &taskData);

    void AddExtDock(const QString &id, const QString &title, const std::function<void()> &showCallback);
    void RemoveExtDock(const QString &id);
    void ShowExtDocksPopup();

    void LoadConsoleUI(qint64 AgentId);
    void LoadTasksOutput() const;
    void LoadFileBrowserUI(qint64 AgentId);
    void LoadProcessBrowserUI(qint64 AgentId);
    void LoadTerminalUI(qint64 AgentId);
    void LoadShellUI(qint64 AgentId);
    void ShowTunnelCreator(qint64 AgentId, bool socks4, bool socks5, bool lportfwd, bool rportfwd);

Q_SIGNALS:
    void SyncedSignal();
    void SyncedOnReloadSignal(QString project);
    void LoadGlobalScriptSignal(QString path);
    void UnloadGlobalScriptSignal(QString path);
    void serverScriptsChanged();

    void agentTickUpdated(qint64 agentId);

    void eventNewAgent(qint64 agentId);
    void eventFileBrowserDisks(qint64 agentId);
    void eventFileBrowserList(qint64 agentId, QString path);
    void eventFileBrowserUpload(qint64 agentId, QString path, QString localFilename);
    void eventProcessBrowserList(qint64 agentId);

public Q_SLOTS:
    void ChannelClose() const;
    void DataHandler(const QByteArray& data);
    void DataHandlerJson(const QJsonObject& jsonObj);

    void OnWebSocketConnected();
    void OnSynced();
    void SetSessionsTableUI() const;
    void SetGraphUI() const;
    void SetTasksUI() const;
    void LoadScriptsUI() const;
    void LoadCodeEditorUI() const;
    void OpenInDevTools(const QString& filePath);
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