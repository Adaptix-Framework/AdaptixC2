#ifndef ADAPTIXCLIENT_ADAPTIXWIDGET_H
#define ADAPTIXCLIENT_ADAPTIXWIDGET_H

#include <main.h>
#include <Client/WebSocketWorker.h>
#include <Client/LastTickWorker.h>
#include <UI/Dialogs/DialogSyncPacket.h>
#include <UI/Widgets/LogsWidget.h>
#include <UI/Widgets/ListenersWidget.h>
#include <UI/Widgets/SessionsTableWidget.h>
#include <UI/Graph/SessionsGraph.h>
#include <UI/Widgets/DownloadsWidget.h>
#include <UI/Widgets/TasksWidget.h>
#include <UI/Widgets/TunnelsWidget.h>
#include <Client/WidgetBuilder.h>
#include <Agent/Agent.h>
#include <Agent/Task.h>

class SessionsTableWidget;
class SessionsGraph;
class LastTickWorker;

class AdaptixWidget : public QWidget
{
Q_OBJECT
    QGridLayout*    mainGridLayout    = nullptr;
    QHBoxLayout*    topHLayout        = nullptr;
    QPushButton*    listenersButton   = nullptr;
    QPushButton*    logsButton        = nullptr;
    QPushButton*    sessionsButton    = nullptr;
    QPushButton*    graphButton       = nullptr;
    QPushButton*    tasksButton        = nullptr;
    QPushButton*    targetsButton     = nullptr;
    QPushButton*    tunnelButton       = nullptr;
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
    QThread*             ChannelThread     = nullptr;
    WebSocketWorker*     ChannelWsWorker   = nullptr;
    QThread*             TickThread        = nullptr;
    LastTickWorker*      TickWorker        = nullptr;
    LogsWidget*          LogsTab           = nullptr;
    ListenersWidget*     ListenersTab      = nullptr;
    SessionsTableWidget* SessionsTablePage = nullptr;
    SessionsGraph*       SessionsGraphPage = nullptr;
    TunnelsWidget*       TunnelsTab        = nullptr;
    DownloadsWidget*     DownloadsTab      = nullptr;
    TasksWidget*         TasksTab          = nullptr;

    QMap<QString, Commander*>     RegisterAgentsCmd;
    QMap<QString, WidgetBuilder*> RegisterAgentsUI;
    QMap<QString, WidgetBuilder*> RegisterListenersUI;
    QMap<QString, QStringList>    LinkListenerAgent;
    QVector<ListenerData>         Listeners;
    QVector<TunnelData>           Tunnels;
    QMap<QString, DownloadData>   Downloads;
    QMap<QString, PivotData>      Pivots;
    QVector<QString>              TasksVector;
    QMap<QString, Task*>          TasksMap;
    QVector<QString>              AgentsVector;
    QMap<QString, Agent*>         AgentsMap;
    QMap<QString, ExtensionFile>  Extensions;

    explicit AdaptixWidget(AuthProfile* authProfile);
    ~AdaptixWidget() override;

    AuthProfile* GetProfile() const;
    void ClearAdaptix();
    void AddTab(QWidget* tab, const QString &title, const QString &icon = "" ) const;
    void RemoveTab(int index) const;
    void AddExtension(ExtensionFile ext);
    void RemoveExtension(const ExtensionFile &ext);
    void Close();

signals:
    void SyncedSignal();

public slots:
    void ChannelClose() const;
    void DataHandler(const QByteArray& data);

    void OnSynced();
    void SetSessionsTableUI() const;
    void SetGraphUI() const;
    void SetTasksUI() const;
    void LoadLogsUI() const;
    void LoadListenersUI() const;
    void LoadTunnelsUI() const;
    void LoadDownloadsUI() const;
    void LoadTasksOutput() const;
    void OnReconnect();
    void LoadConsoleUI(const QString &AgentId);
    void LoadFileBrowserUI(const QString &AgentId);
    void LoadProcessBrowserUI(const QString &AgentId);
};

#endif //ADAPTIXCLIENT_ADAPTIXWIDGET_H