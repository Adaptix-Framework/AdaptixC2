#include <UI/Widgets/AdaptixWidget.h>
#include <Client/Requestor.h>

AdaptixWidget::AdaptixWidget(AuthProfile* authProfile, QThread* channelThread, WebSocketWorker* channelWsWorker)
{
    this->createUI();
    this->ChannelThread = channelThread;
    this->ChannelWsWorker = channelWsWorker;

    LogsTab           = new LogsWidget();
    ListenersTab      = new ListenersWidget(this);
    SessionsTablePage = new SessionsTableWidget(this);
    SessionsGraphPage = new SessionsGraph(this);
    TunnelsTab        = new TunnelsWidget(this);
    DownloadsTab      = new DownloadsWidget(this);
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
    connect( reconnectButton, &QPushButton::clicked, this, &AdaptixWidget::OnReconnect);

    connect( mainTabWidget->tabBar(), &QTabBar::tabCloseRequested, this, &AdaptixWidget::RemoveTab );

    connect( TickThread, &QThread::started, TickWorker, &LastTickWorker::run );

    // connect( ChannelThread,   &QThread::started,              ChannelWsWorker, &WebSocketWorker::run );
    connect( ChannelWsWorker, &WebSocketWorker::received_data,    this, &AdaptixWidget::DataHandler );
    connect( ChannelWsWorker, &WebSocketWorker::websocket_closed, this, &AdaptixWidget::ChannelClose );

    dialogSyncPacket = new DialogSyncPacket();
    dialogSyncPacket->splashScreen->show();

    TickThread->start();
    ChannelThread->start();

    // TODO: Enable menu button
    line_2->setVisible(false);
    targetsButton->setVisible(false);
    credsButton->setVisible(false);
    screensButton->setVisible(false);
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
    topHLayout->addWidget(line_2);
    topHLayout->addWidget(tasksButton);
    topHLayout->addWidget(line_3);
    topHLayout->addWidget(tunnelButton);
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

AuthProfile* AdaptixWidget::GetProfile() const
{
    return this->profile;
}

void AdaptixWidget::RegisterListenerConfig(const QString &fn, const QString &ui)
{
    auto widgetBuilder = new WidgetBuilder(ui.toLocal8Bit() );
    if(widgetBuilder->GetError().isEmpty())
        RegisterListeners[fn] = widgetBuilder;
}

void AdaptixWidget::RegisterAgentConfig(const QString &agentName, const QString &watermark, const QString &handlersJson, const QString &listenersJson)
{
    QJsonParseError parseError;

    QJsonDocument handlersDocument = QJsonDocument::fromJson(handlersJson.toLocal8Bit(), &parseError);
    if (parseError.error != QJsonParseError::NoError && handlersDocument.isObject()) {
        LogError("JSON parse error: %s", parseError.errorString().toStdString().c_str());
        return;
    }
    if(!handlersDocument.isArray()) {
        LogError("Error Listener %s Json Format", agentName.toStdString().c_str());
        return;
    }
    QJsonArray handlersArray = handlersDocument.array();
    for (QJsonValue handlerValue : handlersArray) {
        QJsonObject handlerObj = handlerValue.toObject();
        if (!handlerObj.contains("id")       || !handlerObj["id"].isString())       continue;
        if (!handlerObj.contains("commands") || !handlerObj["commands"].isArray())  continue;
        if (!handlerObj.contains("browsers") || !handlerObj["browsers"].isObject()) continue;

        QString     handlerId      = handlerObj["id"].toString();
        QJsonArray  commandsArray  = handlerObj["commands"].toArray();
        QJsonObject browsersObject = handlerObj["browsers"].toObject();

        QByteArray commandsData = QJsonDocument(commandsArray).toJson();
        auto commander = new Commander();
        bool result = true;
        QString msg = ValidCommandsFile(commandsData, &result);
        if (result) {
            commander->AddRegCommands(commandsData);
        }
        Commanders[agentName][handlerId] = commander;

        BrowsersConfig browsersConfig = {};
        if (browsersObject.contains("file_browser") && browsersObject["file_browser"].isBool())
            browsersConfig.FileBrowser = browsersObject["file_browser"].toBool();
        if (browsersObject.contains("file_browser_disks") && browsersObject["file_browser_disks"].isBool())
            browsersConfig.FileBrowserDisks = browsersObject["file_browser_disks"].toBool();
        if (browsersObject.contains("file_browser_download") && browsersObject["file_browser_download"].isBool())
            browsersConfig.FileBrowserDownload = browsersObject["file_browser_download"].toBool();
        if (browsersObject.contains("file_browser_upload") && browsersObject["file_browser_upload"].isBool())
            browsersConfig.FileBrowserUpload = browsersObject["file_browser_upload"].toBool();
        if (browsersObject.contains("process_browser") && browsersObject["process_browser"].isBool())
            browsersConfig.ProcessBrowser = browsersObject["process_browser"].toBool();
        if (browsersObject.contains("downloads_state") && browsersObject["downloads_state"].isBool())
            browsersConfig.DownloadState = browsersObject["downloads_state"].toBool();
        if (browsersObject.contains("tasks_job_kill") && browsersObject["tasks_job_kill"].isBool())
            browsersConfig.TasksJobKill = browsersObject["tasks_job_kill"].toBool();
        if (browsersObject.contains("sessions_menu_exit") && browsersObject["sessions_menu_exit"].isBool())
            browsersConfig.SessionsMenuExit = browsersObject["sessions_menu_exit"].toBool();

        AgentBrowserConfigs[agentName][handlerId] = browsersConfig;
    }

    QJsonDocument listenersDocument = QJsonDocument::fromJson(listenersJson.toLocal8Bit(), &parseError);
    if (parseError.error != QJsonParseError::NoError && listenersDocument.isObject()) {
        LogError("JSON parse error: %s", parseError.errorString().toStdString().c_str());
        return;
    }
    if(!listenersDocument.isArray()) {
        LogError("Error Listener %s Json Format", agentName.toStdString().c_str());
        return;
    }
    QJsonArray listenersArray = listenersDocument.array();
    for (QJsonValue listenerValue : listenersArray) {
        QJsonObject listenerObj = listenerValue.toObject();
        if (!listenerObj.contains("listener_name") || !listenerObj["listener_name"].isString()) continue;
        if (!listenerObj.contains("configs")       || !listenerObj["configs"].isArray())     continue;

        QString    listenerName   = listenerObj["listener_name"].toString();
        QJsonArray osConfigsArray = listenerObj["configs"].toArray();

        for (QJsonValue osConfigValue : osConfigsArray) {
            QJsonObject osConfigObj = osConfigValue.toObject();
            if (!osConfigObj.contains("operating_system") || !osConfigObj["operating_system"].isString()) continue;
            if (!osConfigObj.contains("handler")          || !osConfigObj["handler"].isString())          continue;
            if (!osConfigObj.contains("generate_ui")      || !osConfigObj["generate_ui"].isObject())      continue;

            QString     operatingSystem = osConfigObj["operating_system"].toString();
            QString     handler         = osConfigObj["handler"].toString();
            QJsonObject uiObj           = osConfigObj["generate_ui"].toObject();

            QByteArray uiData = QJsonDocument(uiObj).toJson();
            auto widgetBuilder = new WidgetBuilder( uiData );
            if(widgetBuilder->GetError().isEmpty()) {
                delete widgetBuilder;
                widgetBuilder = nullptr;
            }

            RegAgentConfig config = {agentName, watermark, listenerName, operatingSystem, handler, widgetBuilder, Commanders[agentName][handler], AgentBrowserConfigs[agentName][handler], true};
            RegisterAgents.push_back(config);
        }
    }
}

void AdaptixWidget::ClearAdaptix()
{
    LogsTab->Clear();
    DownloadsTab->Clear();
    TasksTab->Clear();
    ListenersTab->Clear();
    SessionsGraphPage->Clear();
    SessionsTablePage->Clear();
    TunnelsTab->Clear();

    for (int i = 0; i < RegisterAgents.size(); i++) {
        WidgetBuilder* builder   = RegisterAgents[i].builder;
        RegisterAgents.remove(i);
        i--;
        delete builder;
    }

    for (auto commanderMap : Commanders ) {
        for (auto k : commanderMap.keys()) {
            Commander* commander = commanderMap[k];
            commanderMap.remove(k);
            delete commander;
        }
    }
    Commanders.clear();

    AgentBrowserConfigs.clear();

    for (auto listenerName : RegisterListeners.keys()){
        WidgetBuilder* builder = RegisterListeners[listenerName];
        RegisterListeners.remove(listenerName);
        delete builder;
    }
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



RegAgentConfig AdaptixWidget::GetRegAgent(const QString &agentName, const QString &listenerName, const int os)
{
    QString operatingSystem = "windows";
    if (os == OS_LINUX)
        operatingSystem = "linux";
    else if (os == OS_MAC)
        operatingSystem = "mac";

    QString listener = "";
    for ( auto listenerData : this->Listeners) {
        if ( listenerData.ListenerName == listenerName ) {
            listener = listenerData.ListenerType.split("/")[2];
            break;
        }
    }

    for (auto regAgent : this->RegisterAgents) {
        if (regAgent.agentName == agentName && regAgent.listenerName == listener && regAgent.operatingSystem == operatingSystem)
            return regAgent;
    }

    return {};
}



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

void AdaptixWidget::AddExtension(ExtensionFile ext)
{
    if( Extensions.contains(ext.FilePath) )
        return;

    Extensions[ext.FilePath] = ext;

    if( !synchronized )
        return;

    for (QString agentName : ext.ExCommands.keys()) {
        if (Commanders.contains(agentName)) {
            for (auto commander : Commanders[agentName] ) {
                if (commander) {
                    bool result = commander->AddExtModule(ext.FilePath, ext.Name, ext.ExCommands[agentName], ext.ExConstants);
                    if (result) {
                        for( auto agent : AgentsMap ){
                            if( agent && agent->Console )
                                agent->Console->UpgradeCompleter();
                        }
                    }
                }
            }
        }
    }
}

void AdaptixWidget::RemoveExtension(const ExtensionFile &ext)
{
    Extensions.remove(ext.FilePath);

    for (QString agentName : ext.ExCommands.keys()) {
        if (Commanders.contains(agentName)) {
            for (auto commander : Commanders[agentName] ) {
                if (commander) {
                    commander->RemoveExtModule(ext.FilePath);
                    for( auto agent : AgentsMap ){
                        if( agent && agent->Console )
                            agent->Console->UpgradeCompleter();
                    }
                }
            }
        }
    }
}



/// SLOTS

void AdaptixWidget::OnSynced()
{
    synchronized = true;

    this->SessionsGraphPage->TreeDraw();

    for (auto ext : Extensions) {

        for (QString agentName : ext.ExCommands.keys()) {

            if (Commanders.contains(agentName)) {
                for (auto commander : Commanders[agentName] ) {

                    if (commander) {
                        bool result = commander->AddExtModule(ext.FilePath, ext.Name, ext.ExCommands[agentName], ext.ExConstants);
                        if (result) {
                            for( auto agent : AgentsMap ){
                                if( agent && agent->Console )
                                    agent->Console->UpgradeCompleter();
                            }
                        }
                    }
                }
            }
        }
    }
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

void AdaptixWidget::LoadLogsUI() const
{
    this->AddTab(LogsTab, "Logs", ":/icons/logs");
}

void AdaptixWidget::LoadListenersUI() const
{
    this->AddTab(ListenersTab, "Listeners", ":/icons/listeners");
}

void AdaptixWidget::LoadTunnelsUI() const
{
    this->AddTab(TunnelsTab, "Tunnels", ":/icons/vpn");
}

void AdaptixWidget::LoadDownloadsUI() const
{
    this->AddTab(DownloadsTab, "Downloads", ":/icons/downloads");
}

void AdaptixWidget::LoadTasksOutput() const
{
    this->AddTab(TasksTab->taskOutputConsole, "Task Output", ":/icons/job");
}

void AdaptixWidget::OnReconnect() {

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

void AdaptixWidget::LoadFileBrowserUI(const QString &AgentId)
{
    if( !AgentsMap.contains(AgentId) )
        return;

    auto agent = AgentsMap[AgentId];
    if (agent && agent->browsers.FileBrowser && agent->FileBrowser) {
        auto text = QString("Files [%1]").arg( AgentId );
        this->AddTab(AgentsMap[AgentId]->FileBrowser, text);
    }
}

void AdaptixWidget::LoadProcessBrowserUI(const QString &AgentId)
{
    if( !AgentsMap.contains(AgentId) )
        return;

    auto agent = AgentsMap[AgentId];
    if (agent && agent->browsers.ProcessBrowser && agent->ProcessBrowser) {
        auto text = QString("Processes [%1]").arg( AgentId );
        this->AddTab(AgentsMap[AgentId]->ProcessBrowser, text);
    }
}

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
