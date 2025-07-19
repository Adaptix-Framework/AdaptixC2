#include <QJSEngine>
#include <Agent/Agent.h>
#include <Agent/Task.h>
#include <Workers/LastTickWorker.h>
#include <Workers/WebSocketWorker.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/AxConsoleWidget.h>
#include <UI/Widgets/BrowserFilesWidget.h>
#include <UI/Widgets/BrowserProcessWidget.h>
#include <UI/Widgets/TerminalWidget.h>
#include <UI/Widgets/SessionsTableWidget.h>
#include <UI/Widgets/LogsWidget.h>
#include <UI/Widgets/ListenersWidget.h>
#include <UI/Widgets/DownloadsWidget.h>
#include <UI/Widgets/ScreenshotsWidget.h>
#include <UI/Widgets/CredentialsWidget.h>
#include <UI/Widgets/TasksWidget.h>
#include <UI/Widgets/TunnelsWidget.h>
#include <UI/Graph/SessionsGraph.h>
#include <UI/Dialogs/DialogSyncPacket.h>
#include <UI/Dialogs/DialogTunnel.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/TunnelEndpoint.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/AxCommandWrappers.h>

AdaptixWidget::AdaptixWidget(AuthProfile* authProfile, QThread* channelThread, WebSocketWorker* channelWsWorker)
{
    this->createUI();
    this->ChannelThread   = channelThread;
    this->ChannelWsWorker = channelWsWorker;

    ScriptManager = new AxScriptManager(this, this);

    AxConsoleTab      = new AxConsoleWidget(ScriptManager, this);
    LogsTab           = new LogsWidget();
    ListenersTab      = new ListenersWidget(this);
    SessionsTablePage = new SessionsTableWidget(this);
    SessionsGraphPage = new SessionsGraph(this);
    TunnelsTab        = new TunnelsWidget(this);
    DownloadsTab      = new DownloadsWidget(this);
    ScreenshotsTab    = new ScreenshotsWidget(this);
    CredentialsTab    = new CredentialsWidget(this);
    TasksTab          = new TasksWidget(this);

    mainStackedWidget->addWidget(SessionsTablePage);
    mainStackedWidget->addWidget(SessionsGraphPage);
    mainStackedWidget->addWidget(TasksTab);

    this->SetSessionsTableUI();
    this->LoadLogsUI();

    profile = authProfile;

    TickThread = new QThread;
    TickWorker = new LastTickWorker( this );
    TickWorker->moveToThread( TickThread );

    connect( this, &AdaptixWidget::SyncedSignal, this, &AdaptixWidget::OnSynced);

    connect( logsButton,      &QPushButton::clicked, this, &AdaptixWidget::LoadLogsUI);
    connect( listenersButton, &QPushButton::clicked, this, &AdaptixWidget::LoadListenersUI);
    connect( sessionsButton,  &QPushButton::clicked, this, &AdaptixWidget::SetSessionsTableUI);
    connect( graphButton,     &QPushButton::clicked, this, &AdaptixWidget::SetGraphUI);
    connect( tasksButton,     &QPushButton::clicked, this, &AdaptixWidget::SetTasksUI);
    connect( tunnelButton,    &QPushButton::clicked, this, &AdaptixWidget::LoadTunnelsUI);
    connect( downloadsButton, &QPushButton::clicked, this, &AdaptixWidget::LoadDownloadsUI);
    connect( screensButton,   &QPushButton::clicked, this, &AdaptixWidget::LoadScreenshotsUI);
    connect( credsButton,     &QPushButton::clicked, this, &AdaptixWidget::LoadCredentialsUI);
    connect( reconnectButton, &QPushButton::clicked, this, &AdaptixWidget::OnReconnect);

    connect( mainTabWidget->tabBar(), &QTabBar::tabCloseRequested, this, &AdaptixWidget::RemoveTab );

    connect( TickThread, &QThread::started, TickWorker, &LastTickWorker::run );

    connect( ChannelWsWorker, &WebSocketWorker::received_data,    this, &AdaptixWidget::DataHandler );
    connect( ChannelWsWorker, &WebSocketWorker::websocket_closed, this, &AdaptixWidget::ChannelClose );

    dialogSyncPacket = new DialogSyncPacket();
    dialogSyncPacket->splashScreen->show();

    TickThread->start();
    ChannelThread->start();

    /// TODO: Enable menu button
    targetsButton->setVisible(false);
    keysButton->setVisible(false);

    HttpReqSync( *profile );
}

AdaptixWidget::~AdaptixWidget() = default;

void AdaptixWidget::createUI()
{
    listenersButton = new QPushButton( QIcon(":/icons/listeners"), "", this );
    listenersButton->setIconSize( QSize( 24,24 ));
    listenersButton->setFixedSize(37, 28);
    listenersButton->setToolTip("Listeners & Sites");

    logsButton = new QPushButton(QIcon(":/icons/logs"), "", this );
    logsButton->setIconSize( QSize( 24,24 ));
    logsButton->setFixedSize(37, 28);
    logsButton->setToolTip("Logs");

    line_1 = new QFrame(this);
    line_1->setFrameShape(QFrame::VLine);
    line_1->setMinimumHeight(25);

    sessionsButton = new QPushButton( QIcon(":/icons/format_list"), "", this );
    sessionsButton->setIconSize( QSize( 24,24 ));
    sessionsButton->setFixedSize(37, 28);
    sessionsButton->setToolTip("Session table");

    graphButton = new QPushButton( QIcon(":/icons/graph"), "", this );
    graphButton->setIconSize( QSize( 24,24 ));
    graphButton->setFixedSize(37, 28);
    graphButton->setToolTip("Session graph");

    targetsButton = new QPushButton( QIcon(":/icons/devices"), "", this );
    targetsButton->setIconSize( QSize( 24,24 ));
    targetsButton->setFixedSize(37, 28);
    targetsButton->setToolTip("Targets table");

    line_2 = new QFrame(this);
    line_2->setFrameShape(QFrame::VLine);
    line_2->setMinimumHeight(25);

    tasksButton = new QPushButton(QIcon(":/icons/job"), "", this );
    tasksButton->setIconSize(QSize(24, 24 ));
    tasksButton->setFixedSize(37, 28);
    tasksButton->setToolTip("Jobs & Tasks");

    tunnelButton = new QPushButton( QIcon(":/icons/vpn"), "", this );
    tunnelButton->setIconSize( QSize( 24,24 ));
    tunnelButton->setFixedSize(37, 28);
    tunnelButton->setToolTip("Tunnels table");

    line_3 = new QFrame(this);
    line_3->setFrameShape(QFrame::VLine);
    line_3->setMinimumHeight(25);

    downloadsButton = new QPushButton( QIcon(":/icons/downloads"), "", this );
    downloadsButton->setIconSize( QSize( 24,24 ));
    downloadsButton->setFixedSize(37, 28);
    downloadsButton->setToolTip("Downloads");

    credsButton = new QPushButton( QIcon(":/icons/key"), "", this );
    credsButton->setIconSize( QSize( 24,24 ));
    credsButton->setFixedSize(37, 28);
    credsButton->setToolTip("Credentials");

    screensButton = new QPushButton( QIcon(":/icons/picture"), "", this );
    screensButton->setIconSize( QSize( 24,24 ));
    screensButton->setFixedSize(37, 28);
    screensButton->setToolTip("Screens");

    keysButton = new QPushButton( QIcon(":/icons/keyboard"), "", this );
    keysButton->setIconSize( QSize( 24,24 ));
    keysButton->setFixedSize(37, 28);
    keysButton->setToolTip("Keystrokes");

    line_4 = new QFrame(this);
    line_4->setFrameShape(QFrame::VLine);
    line_4->setMinimumHeight(25);

    reconnectButton = new QPushButton(QIcon(":/icons/link"), "");
    reconnectButton->setIconSize( QSize( 24,24 ));
    reconnectButton->setFixedSize(37, 28);
    reconnectButton->setToolTip("Reconnect to C2");
    QIcon onReconnectButton = RecolorIcon(reconnectButton->icon(), COLOR_NeonGreen);
    reconnectButton->setIcon(onReconnectButton);

    horizontalSpacer1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    topHLayout = new QHBoxLayout();
    topHLayout->setContentsMargins(5, 5, 0, 5);
    topHLayout->setSpacing(10);
    topHLayout->setAlignment(Qt::AlignLeft);

    topHLayout->addWidget(listenersButton);
    topHLayout->addWidget(logsButton);
    topHLayout->addWidget(line_1);
    topHLayout->addWidget(sessionsButton);
    topHLayout->addWidget(graphButton);
    topHLayout->addWidget(targetsButton);
    topHLayout->addWidget(tasksButton);
    topHLayout->addWidget(line_2);
    topHLayout->addWidget(tunnelButton);
    topHLayout->addWidget(line_3);
    topHLayout->addWidget(downloadsButton);
    topHLayout->addWidget(credsButton);
    topHLayout->addWidget(screensButton);
    topHLayout->addWidget(keysButton);
    topHLayout->addWidget(line_4);
    topHLayout->addWidget(reconnectButton);
    topHLayout->addItem(horizontalSpacer1);

    mainStackedWidget = new QStackedWidget(this);

    mainTabWidget = new QTabWidget(this);
    mainTabWidget->setCurrentIndex( 0 );
    mainTabWidget->setMovable( false );
    mainTabWidget->setTabsClosable( true );

    mainVSplitter = new QSplitter(Qt::Vertical, this);
    mainVSplitter->setContentsMargins(0, 0, 0, 0);
    mainVSplitter->setHandleWidth(3);
    mainVSplitter->setVisible(true);
    mainVSplitter->addWidget(mainStackedWidget);
    mainVSplitter->addWidget(mainTabWidget);
    mainVSplitter->setSizes(QList<int>({200, 200}));

    mainGridLayout = new QGridLayout(this );
    mainGridLayout->setContentsMargins(0, 0, 0, 0);
    mainGridLayout->setVerticalSpacing(0);
    mainGridLayout->addLayout(topHLayout, 0, 0, 1, 1);
    mainGridLayout->addWidget(mainVSplitter, 1, 0, 1, 1);

    this->setLayout(mainGridLayout);
}

/// MAIN

AuthProfile* AdaptixWidget::GetProfile() const { return this->profile; }

void AdaptixWidget::AddTab(QWidget *tab, const QString &title, const QString &icon) const
{
    if ( mainTabWidget->count() == 0 )
        mainVSplitter->setSizes(QList<int>() << 100 << 200);
    else if ( mainTabWidget->count() == 1 )
        mainTabWidget->setMovable(true);

    int id = mainTabWidget->addTab( tab, title );

    mainTabWidget->setIconSize( QSize( 17, 17 ) );
    mainTabWidget->setTabIcon(id, QIcon(icon));
    mainTabWidget->setCurrentIndex( id );
}

void AdaptixWidget::RemoveTab(int index) const
{
    if (index == -1)
        return;

    mainTabWidget->removeTab(index);

    if (mainTabWidget->count() == 0)
        mainVSplitter->setSizes(QList<int>() << 0);
    else if (mainTabWidget->count() == 1)
        mainTabWidget->setMovable(false);
}

bool AdaptixWidget::AddExtension(ExtensionFile* ext)
{
    if (ScriptManager->ScriptList().contains(ext->FilePath)) {
        ext->Enabled = false;
        ext->Message = "Script already loaded";
        return false;
    }

    if( !synchronized ) {
        ext->Enabled = false;
        ext->Message = "C2 not synchronized";
        return false;
    }

    return ScriptManager->ScriptAdd(ext);
}

void AdaptixWidget::RemoveExtension(const ExtensionFile &ext)
{
    if (!ScriptManager->ScriptList().contains(ext.FilePath))
        return;

    return ScriptManager->ScriptRemove(ext);
}

void AdaptixWidget::Close()
{
    TickThread->quit();
    TickThread->wait();
    delete TickThread;

    ChannelThread->quit();
    ChannelThread->wait();
    delete ChannelThread;

    ChannelWsWorker->webSocket->close();

    this->ClearAdaptix();
}

void AdaptixWidget::ClearAdaptix()
{
    AxConsoleTab->OutputClear();
    LogsTab->Clear();
    DownloadsTab->Clear();
    ScreenshotsTab->Clear();
    TasksTab->Clear();
    ListenersTab->Clear();
    SessionsGraphPage->Clear();
    SessionsTablePage->Clear();
    TunnelsTab->Clear();
    CredentialsTab->Clear();

    for (auto tunnelId : ClientTunnels.keys()) {
        auto tunnel = ClientTunnels[tunnelId];
        ClientTunnels.remove(tunnelId);
        tunnel->Stop();
        delete tunnel;
    }
    ClientTunnels.clear();

    ScriptManager->Clear();

    for (auto regAgent : RegisterAgents)
        delete regAgent.commander;

    RegisterAgents.clear();
}

/// REGISTER

void AdaptixWidget::RegisterListenerConfig(const QString &fn, const QString &ax_script) { ScriptManager->ListenerScriptAdd(fn, ax_script); }

void AdaptixWidget::RegisterAgentConfig(const QString &agentName, const QString &ax_script, const QStringList &listeners)
{
    ScriptManager->AgentScriptAdd(agentName, ax_script);

    QJSEngine* engine = ScriptManager->AgentScriptEngine(agentName);
    if (!engine)
        return;

    QJSValue func = engine->globalObject().property("RegisterCommands");
    if (!func.isCallable()) {
        ScriptManager->consolePrintError(agentName + " - function RegisterCommands is not registered");
        return;
    }

    for (auto listener : listeners) {

        QJSValueList args;
        args << QJSValue(listener);
        QJSValue registerResult = func.call(args);
        if (registerResult.isError()) {
            QString error = QStringLiteral("%1\n  at line %2 in %3\n  stack: %4").arg(registerResult.toString()).arg(registerResult.property("lineNumber").toInt()).arg(agentName).arg(registerResult.property("stack").toString());
            ScriptManager->consolePrintError(error);
            return;
        }
        if (!registerResult.isObject()) {
            ScriptManager->consolePrintError(agentName + " - function RegisterCommands must return CommandsGroup objects");
            return;
        }

        QJSValue commands_windows = registerResult.property("commands_windows");
        if ( !commands_windows.isUndefined() && commands_windows.isQObject()) {
            QObject* objPanel = commands_windows.toQObject();
            auto* wrapper = dynamic_cast<AxCommandGroupWrapper*>(objPanel);
            if (wrapper) {
                CommandsGroup commandsGroup = {};
                commandsGroup.groupName = wrapper->getName();
                commandsGroup.commands  = wrapper->getCommands();
                commandsGroup.engine    = wrapper->getEngine();
                commandsGroup.filepath  = "";

                Commander* commander = new Commander();
                commander->AddRegCommands(commandsGroup);

                RegAgentConfig config = {agentName, listener, OS_WINDOWS, commander, {}, true};
                RegisterAgents.push_back(config);
            }
            else {
                ScriptManager->consolePrintError(agentName + " - commands_windows must return CommandsGroup object");
            }
        }

        QJSValue commands_linux = registerResult.property("commands_linux");
        if ( !commands_linux.isUndefined() && commands_linux.isQObject()) {
            QObject* objPanel = commands_linux.toQObject();
            auto* wrapper = dynamic_cast<AxCommandGroupWrapper*>(objPanel);
            if (wrapper) {
                CommandsGroup commandsGroup = {};
                commandsGroup.groupName = wrapper->getName();
                commandsGroup.commands  = wrapper->getCommands();
                commandsGroup.engine    = wrapper->getEngine();
                commandsGroup.filepath  = "";

                Commander* commander = new Commander();
                commander->AddRegCommands(commandsGroup);

                RegAgentConfig config = {agentName, listener, OS_LINUX, commander, {}, true};
                RegisterAgents.push_back(config);
            }
            else {
                ScriptManager->consolePrintError(agentName + " - commands_linux must return CommandsGroup object");
            }
        }

        QJSValue commands_macos = registerResult.property("commands_macos");
        if ( !commands_macos.isUndefined() && commands_macos.isQObject()) {
            QObject* objPanel = commands_macos.toQObject();
            auto* wrapper = dynamic_cast<AxCommandGroupWrapper*>(objPanel);
            if (wrapper) {
                CommandsGroup commandsGroup = {};
                commandsGroup.groupName = wrapper->getName();
                commandsGroup.commands  = wrapper->getCommands();
                commandsGroup.engine    = wrapper->getEngine();
                commandsGroup.filepath  = "";

                Commander* commander = new Commander();
                commander->AddRegCommands(commandsGroup);

                RegAgentConfig config = {agentName, listener, OS_MAC, commander, {}, true};
                RegisterAgents.push_back(config);
            }
            else {
                ScriptManager->consolePrintError(agentName + " - commands_macos must return CommandsGroup object");
            }
        }
    }
}

QList<QString> AdaptixWidget::GetAgentNames(const QString &listenerType) const
{
    QSet<QString> names;
    for (auto regAgent : this->RegisterAgents) {
        if (regAgent.listenerType == listenerType)
            names.insert(regAgent.name);
    }
    return names.values();
}

RegAgentConfig AdaptixWidget::GetRegAgent(const QString &agentName, const QString &listenerName, const int os)
{
    if (os == OS_WINDOWS || os == OS_LINUX || os == OS_MAC) {
        QString listener = "";
        for ( auto listenerData : this->Listeners) {
            if ( listenerData.ListenerName == listenerName ) {
                listener = listenerData.ListenerFullName.split("/")[2];
                break;
            }
        }

        for (auto regAgent : this->RegisterAgents) {
            if (regAgent.name == agentName && regAgent.listenerType == listener && regAgent.os == os)
                return regAgent;
        }
    }

    return {};
}

QList<Commander*> AdaptixWidget::GetCommanders(const QStringList &listeners, const QStringList &agents, const QList<int> &os) const
{
    QList<Commander*> commanders;
    for (auto regAgent : this->RegisterAgents) {
        if ( !agents.contains(regAgent.name) ) continue;

        if ( !listeners.empty() && !listeners.contains(regAgent.listenerType)) continue;

        if ( !os.empty() && !os.contains(regAgent.os) ) continue;

        commanders.append(regAgent.commander);
    }
    return commanders;
}

QList<Commander*> AdaptixWidget::GetCommandersAll() const
{
    QList<Commander*> commanders;
    for (auto regAgent : this->RegisterAgents)
        commanders.append(regAgent.commander);
    return commanders;
}

void AdaptixWidget::PostHookProcess(QJsonObject jsonHookObj)
{
    QString hookId = jsonHookObj["a_hook_id"].toString();
    bool completed = jsonHookObj["a_completed"].toBool();

    if (PostHooksJS.contains(hookId)) {
        PostHook post_hooks = PostHooksJS[hookId];
        if (completed)
            PostHooksJS.remove(hookId);

        auto jsEngine = ScriptManager->GetEngine(post_hooks.engineName);
        if (jsEngine && post_hooks.hook.isCallable()) {

            int jobIndex = jsonHookObj["a_job_index"].toDouble();

            QJsonObject obj;
            obj["agent"]     = jsonHookObj["a_id"].toString();;
            obj["message"]   = jsonHookObj["a_message"].toString();
            obj["text"]      = jsonHookObj["a_text"].toString();
            obj["completed"] = completed;
            obj["index"]     = jobIndex;

            int msgType = jsonHookObj["a_msg_type"].toDouble();
            if (msgType == CONSOLE_OUT_LOCAL_INFO || msgType == CONSOLE_OUT_INFO)
                obj["type"] = "info";
            else if (msgType == CONSOLE_OUT_LOCAL_ERROR || msgType == CONSOLE_OUT_ERROR)
                obj["type"] = "error";
            else if (msgType == CONSOLE_OUT_LOCAL_SUCCESS || msgType == CONSOLE_OUT_SUCCESS)
                obj["type"] = "success";
            else
                obj["type"] = "";

            QJSValue result = post_hooks.hook.call(QJSValueList() << jsEngine->toScriptValue(obj));
            if (result.isObject()) {
                QJsonObject modifiedObj = result.toVariant().toJsonObject();

                if (modifiedObj.contains("message") && modifiedObj["message"].isString())
                    jsonHookObj["a_message"] = modifiedObj["message"].toString();
                if (modifiedObj.contains("text") && modifiedObj["text"].isString())
                    jsonHookObj["a_text"] = modifiedObj["text"].toString();
                if (modifiedObj.contains("type") && modifiedObj["type"].isString()) {
                    QString modifiedType = modifiedObj["type"].toString();
                    if (modifiedType == "info")
                        jsonHookObj["a_msg_type"] = CONSOLE_OUT_INFO;
                    else if (modifiedType == "error")
                        jsonHookObj["a_msg_type"] = CONSOLE_OUT_ERROR;
                    else if (modifiedType == "success")
                        jsonHookObj["a_msg_type"] = CONSOLE_OUT_SUCCESS;
                    else
                        jsonHookObj["a_msg_type"] = CONSOLE_OUT;
                }
            }
        }
    }

    QByteArray jsonData = QJsonDocument(jsonHookObj).toJson();

    QString message = "";
    bool ok = false;
    bool result = HttpReqTasksHook(jsonData, *profile, &message, &ok);
    if( !result ) {
        MessageError("Server is not responding");
        return;
    }
    if (!ok) MessageError(message);
}

/// SHOW PANELS

void AdaptixWidget::LoadConsoleUI(const QString &AgentId)
{
    if( !AgentsMap.contains(AgentId) )
        return;

    auto agent = AgentsMap[AgentId];
    if (agent && agent->Console) {
        auto text = QString("Console [%1]").arg( AgentId );
        this->AddTab(AgentsMap[AgentId]->Console, text);
        AgentsMap[AgentId]->Console->InputFocus();
    }

}

void AdaptixWidget::LoadTasksOutput() const { this->AddTab(TasksTab->taskOutputConsole, "Task Output", ":/icons/job"); }

void AdaptixWidget::LoadFileBrowserUI(const QString &AgentId)
{
    if( !AgentsMap.contains(AgentId) )
        return;

    auto agent = AgentsMap[AgentId];
    if (agent && agent->FileBrowser) {
        auto text = QString("Files [%1]").arg( AgentId );
        this->AddTab(AgentsMap[AgentId]->FileBrowser, text);
    }
}

void AdaptixWidget::LoadProcessBrowserUI(const QString &AgentId)
{
    if( !AgentsMap.contains(AgentId) )
        return;

    auto agent = AgentsMap[AgentId];
    if (agent && agent->ProcessBrowser) {
        auto text = QString("Processes [%1]").arg( AgentId );
        this->AddTab(AgentsMap[AgentId]->ProcessBrowser, text);
    }
}

void AdaptixWidget::LoadTerminalUI(const QString &AgentId)
{
    if( !AgentsMap.contains(AgentId) )
        return;

    auto agent = AgentsMap[AgentId];
    if (agent && agent->Terminal) {
        auto text = QString("Terminal [%1]").arg( AgentId );
        this->AddTab(AgentsMap[AgentId]->Terminal, text);
    }
}

void AdaptixWidget::ShowTunnelCreator(const QString &AgentId, const bool socks4, const bool socks5, const bool lportfwd, const bool rportfwd)
{
    DialogTunnel* dialogTunnel = new DialogTunnel(AgentId, socks4, socks5, lportfwd, rportfwd);

    while (true) {
        dialogTunnel->StartDialog();
        if (dialogTunnel->IsValid())
            break;

        QString msg = dialogTunnel->GetMessage();
        if (msg.isEmpty()) {
            delete dialogTunnel;
            return;
        }

        MessageError(msg);
    }

    QString    tunnelType = dialogTunnel->GetTunnelType();
    QString    endpoint   = dialogTunnel->GetEndpoint();
    QByteArray tunnelData = dialogTunnel->GetTunnelData();

    if ( endpoint == "Teamserver" ) {
        QString message = "";
        bool ok = false;
        bool result = HttpReqTunnelStartServer(tunnelType, tunnelData, *profile, &message, &ok);
        if( !result ) {
            MessageError("Server is not responding");
            delete dialogTunnel;
            return;
        }
        if (!ok) MessageError(message);
    }
    else {
        auto tunnelEndpoint = new TunnelEndpoint();
        bool started = tunnelEndpoint->StartTunnel(profile, tunnelType, tunnelData);
        if (started) {
            QString message = "";
            bool ok = false;
            bool result = HttpReqTunnelStartServer(tunnelType, tunnelData, *profile, &message, &ok);
            if( !result ) {
                MessageError("Server is not responding");
                delete tunnelEndpoint;
                delete dialogTunnel;
                return;
            }

            if ( !ok ) {
                MessageError(message);
                delete tunnelEndpoint;
                delete dialogTunnel;
                return;
            }
            QString tunnelId = message;

            tunnelEndpoint->SetTunnelId(tunnelId);
            this->ClientTunnels[tunnelId] = tunnelEndpoint;
            MessageSuccess("Tunnel " + tunnelId + " started");
        }
        else {
            delete tunnelEndpoint;
        }
    }
    delete dialogTunnel;
}

/// SLOTS

void AdaptixWidget::ChannelClose() const
{
    QIcon onReconnectButton = RecolorIcon(QIcon(":/icons/unlink"), COLOR_ChiliPepper);
    reconnectButton->setIcon(onReconnectButton);
    ChannelThread->quit();
}

void AdaptixWidget::DataHandler(const QByteArray &data)
{
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &parseError);

    if ( parseError.error != QJsonParseError::NoError || !jsonDoc.isObject() ) {
        LogError("Error parsing JSON data: %s", parseError.errorString().toStdString().c_str());
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();
    if( !this->isValidSyncPacket(jsonObj) ) {
        LogError("Invalid SyncPacket");
        return;
    }

    this->processSyncPacket(jsonObj);
}

void AdaptixWidget::OnSynced()
{
    synchronized = true;

    this->SessionsGraphPage->TreeDraw();

    emit SyncedOnReloadSignal(profile->GetProject());
}

void AdaptixWidget::SetSessionsTableUI() const
{
    mainStackedWidget->setCurrentIndex(0);
    int index = mainTabWidget->indexOf(TasksTab->taskOutputConsole);
    if (index < 0)
        return;

    mainTabWidget->removeTab(index);
}

void AdaptixWidget::SetGraphUI() const
{
    mainStackedWidget->setCurrentIndex(1);
    int index = mainTabWidget->indexOf(TasksTab->taskOutputConsole);
    if (index < 0)
        return;

    mainTabWidget->removeTab(index);
}

void AdaptixWidget::SetTasksUI() const
{
    mainStackedWidget->setCurrentIndex(2);
    this->AddTab(TasksTab->taskOutputConsole, "Task Output", ":/icons/job");
}

void AdaptixWidget::LoadAxConsoleUI() const { this->AddTab(AxConsoleTab, "AxScript Console", ":/icons/code_blocks"); }

void AdaptixWidget::LoadLogsUI() const { this->AddTab(LogsTab, "Logs", ":/icons/logs"); }

void AdaptixWidget::LoadListenersUI() const { this->AddTab(ListenersTab, "Listeners", ":/icons/listeners"); }

void AdaptixWidget::LoadTunnelsUI() const { this->AddTab(TunnelsTab, "Tunnels", ":/icons/vpn"); }

void AdaptixWidget::LoadDownloadsUI() const { this->AddTab(DownloadsTab, "Downloads", ":/icons/downloads"); }

void AdaptixWidget::LoadScreenshotsUI() const { this->AddTab(ScreenshotsTab, "Screenshots", ":/icons/picture"); }

void AdaptixWidget::LoadCredentialsUI() const { this->AddTab(CredentialsTab, "Credentials", ":/icons/key"); }

void AdaptixWidget::OnReconnect()
{
    if (ChannelThread->isRunning()) {
        bool result = HttpReqJwtUpdate(profile);
        if (!result) {
            MessageError("Login failure");
            return;
        }
    }
    else {
        bool result = HttpReqLogin(profile);
        if (!result) {
            MessageError("Login failure");
            return;
        }

        this->ClearAdaptix();

        ChannelThread->start();

        QIcon onReconnectButton = RecolorIcon(QIcon(":/icons/link"), COLOR_NeonGreen);
        reconnectButton->setIcon(onReconnectButton);

        HttpReqSync( *profile );
    }
}

