#ifndef ADAPTIXCLIENT_ADAPTIXWIDGET_H
#define ADAPTIXCLIENT_ADAPTIXWIDGET_H

#include <main.h>
#include <Client/WebSocketWorker.h>
#include <Client/LastTickWorker.h>
#include <UI/Dialogs/DialogSyncPacket.h>
#include <UI/Widgets/LogsWidget.h>
#include <UI/Widgets/ListenersWidget.h>
#include <UI/Widgets/SessionsTableWidget.h>
#include <UI/Widgets/DownloadsWidget.h>
#include <UI/Widgets/TasksWidget.h>
#include <UI/Widgets/TunnelsWidget.h>
#include <Client/WidgetBuilder.h>
#include <Agent/Agent.h>

class SessionsTableWidget;
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
    bool isValidSyncPacket(QJsonObject jsonObj);
    void processSyncPacket(QJsonObject jsonObj);

public:
    QThread*             ChannelThread     = nullptr;
    WebSocketWorker*     ChannelWsWorker   = nullptr;
    QThread*             TickThread        = nullptr;
    LastTickWorker*      TickWorker        = nullptr;
    LogsWidget*          LogsTab           = nullptr;
    ListenersWidget*     ListenersTab      = nullptr;
    SessionsTableWidget* SessionsTablePage = nullptr;
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
    QVector<QString>              TasksVector;
    QMap<QString, TaskData>       TasksMap;
    QMap<QString, Agent*>         Agents;
    QMap<QString, ExtensionFile>  Extensions;

    explicit AdaptixWidget(AuthProfile* authProfile);
    ~AdaptixWidget();

    AuthProfile* GetProfile();
    void ClearAdaptix();
    void AddTab(QWidget* tab, QString title, QString icon = "" );
    void RemoveTab(int index);
    void AddExtension(ExtensionFile ext);
    void RemoveExtension(ExtensionFile ext);
    void Close();

signals:
    void SyncedSignal();

public slots:
    void ChannelClose();
    void DataHandler(const QByteArray& data);

    void OnSynced();
    void SetSessionsTableUI();
    void SetTasksUI();
    void LoadLogsUI();
    void LoadListenersUI();
    void LoadTunnelsUI();
    void LoadDownloadsUI();
    void LoadTasksOutput();
    void OnReconnect();
    void LoadConsoleUI(QString AgentId);
    void LoadFileBrowserUI(QString AgentId);
    void LoadProcessBrowserUI(QString AgentId);
};

#endif //ADAPTIXCLIENT_ADAPTIXWIDGET_H