#include <UI/Widgets/AdaptixWidget.h>

AdaptixWidget::AdaptixWidget(AuthProfile authProfile) {
    this->createUI();

    this->LoadLogsUI();

    profile = authProfile;

    ChannelThread = new QThread;
    ChannelWsWorker = new WebSocketWorker( authProfile );
    ChannelWsWorker->moveToThread( ChannelThread );


    /// For test
    auto logsTab2 = new LogsWidget;
    this->AddTab(logsTab2, "Logs Tab2", ":/icons/picture");
    //

    connect( mainTabWidget->tabBar(), &QTabBar::tabCloseRequested, this, &AdaptixWidget::RemoveTab );
    connect( logsButton, &QPushButton::clicked, this, &AdaptixWidget::LoadLogsUI);

    connect( ChannelThread, &QThread::started, ChannelWsWorker, &WebSocketWorker::run );
    connect( ChannelWsWorker, &WebSocketWorker::received_data, this, &AdaptixWidget::DataHandler );
    connect( ChannelWsWorker, &WebSocketWorker::websocket_closed, this, &AdaptixWidget::ChannelClose );

    ChannelThread->start();



    // TODO: Enable menu button
    listenersButton->setEnabled(false);
    sessionsButton->setEnabled(false);
    graphButton->setEnabled(false);
    targetsButton->setEnabled(false);
    jobsButton->setEnabled(false);
    proxyButton->setEnabled(false);
    downloadsButton->setEnabled(false);
    credsButton->setEnabled(false);
    screensButton->setEnabled(false);
    keysButton->setEnabled(false);
    reconnectButton->setEnabled(false);
    //

}

void AdaptixWidget::createUI() {
    listenersButton = new QPushButton( QIcon(":/icons/listeners"), "", this );
    listenersButton->setObjectName( QString::fromUtf8( "ListenersButton" ) );
    listenersButton->setIconSize( QSize( 24,24 ));
    listenersButton->setFixedSize(37, 28);
    listenersButton->setToolTip("Listeners & Sites");

    logsButton = new QPushButton(QIcon(":/icons/logs"), "", this );
    logsButton->setObjectName( QString::fromUtf8( "LogsButton" ) );
    logsButton->setIconSize( QSize( 24,24 ));
    logsButton->setFixedSize(37, 28);
    logsButton->setToolTip("Logs");

    line_1 = new QFrame(this);
    line_1->setFrameShape(QFrame::VLine);
    line_1->setMinimumHeight(25);

    sessionsButton = new QPushButton( QIcon(":/icons/format_list"), "", this );
    sessionsButton->setObjectName( QString::fromUtf8( "SessionsButton" ) );
    sessionsButton->setIconSize( QSize( 24,24 ));
    sessionsButton->setFixedSize(37, 28);
    sessionsButton->setToolTip("Session table");

    graphButton = new QPushButton( QIcon(":/icons/graph"), "", this );
    graphButton->setObjectName( QString::fromUtf8( "GraphButton" ) );
    graphButton->setIconSize( QSize( 24,24 ));
    graphButton->setFixedSize(37, 28);
    graphButton->setToolTip("Session graph");

    targetsButton = new QPushButton( QIcon(":/icons/devices"), "", this );
    targetsButton->setObjectName( QString::fromUtf8( "TargetsButton" ) );
    targetsButton->setIconSize( QSize( 24,24 ));
    targetsButton->setFixedSize(37, 28);
    targetsButton->setToolTip("Targets table");

    line_2 = new QFrame(this);
    line_2->setFrameShape(QFrame::VLine);
    line_2->setMinimumHeight(25);

    jobsButton = new QPushButton( QIcon(":/icons/job"), "", this );
    jobsButton->setObjectName( QString::fromUtf8( "JobsButton" ) );
    jobsButton->setIconSize( QSize( 24,24 ));
    jobsButton->setFixedSize(37, 28);
    jobsButton->setToolTip("Jobs & Tasks");

    proxyButton = new QPushButton( QIcon(":/icons/vpn"), "", this );
    proxyButton->setObjectName( QString::fromUtf8( "ProxyButton" ) );
    proxyButton->setIconSize( QSize( 24,24 ));
    proxyButton->setFixedSize(37, 28);
    proxyButton->setToolTip("Proxy tables");

    line_3 = new QFrame(this);
    line_3->setFrameShape(QFrame::VLine);
    line_3->setMinimumHeight(25);

    downloadsButton = new QPushButton( QIcon(":/icons/downloads"), "", this );
    downloadsButton->setObjectName( QString::fromUtf8( "DownloadsButton" ) );
    downloadsButton->setIconSize( QSize( 24,24 ));
    downloadsButton->setFixedSize(37, 28);
    downloadsButton->setToolTip("Downloads");

    credsButton = new QPushButton( QIcon(":/icons/key"), "", this );
    credsButton->setObjectName( QString::fromUtf8( "CredentialsButton" ) );
    credsButton->setIconSize( QSize( 24,24 ));
    credsButton->setFixedSize(37, 28);
    credsButton->setToolTip("Credentials");

    screensButton = new QPushButton( QIcon(":/icons/picture"), "", this );
    screensButton->setObjectName( QString::fromUtf8( "ScreensButton" ) );
    screensButton->setIconSize( QSize( 24,24 ));
    screensButton->setFixedSize(37, 28);
    screensButton->setToolTip("Screens");

    keysButton = new QPushButton( QIcon(":/icons/keyboard"), "", this );
    keysButton->setObjectName( QString::fromUtf8( "KeystrokesButton" ) );
    keysButton->setIconSize( QSize( 24,24 ));
    keysButton->setFixedSize(37, 28);
    keysButton->setToolTip("Keystrokes");

    line_4 = new QFrame(this);
    line_4->setFrameShape(QFrame::VLine);
    line_4->setMinimumHeight(25);

    reconnectButton = new QPushButton(QIcon(":/icons/link"), "");
    reconnectButton->setObjectName("ReconnectButton");
    reconnectButton->setIconSize( QSize( 24,24 ));
    reconnectButton->setFixedSize(37, 28);
    keysButton->setToolTip("Reconnect to C2");

    horizontalSpacer1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);


    topHLayout = new QHBoxLayout;
    topHLayout->setObjectName(QString::fromUtf8("TopLayout" ) );
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
    topHLayout->addWidget(jobsButton);
    topHLayout->addWidget(proxyButton);
    topHLayout->addWidget(line_3);
    topHLayout->addWidget(downloadsButton);
    topHLayout->addWidget(credsButton);
    topHLayout->addWidget(screensButton);
    topHLayout->addWidget(keysButton);
    topHLayout->addWidget(line_4);
    topHLayout->addWidget(reconnectButton);
    topHLayout->addItem(horizontalSpacer1);


    // TODO: top widget - Stack ???
    QTextEdit *textEdit1 = new QTextEdit(this);
    //

    mainTabWidget = new QTabWidget(this);
    mainTabWidget->setObjectName( QString::fromUtf8( "MainTabWidget" ) );
    mainTabWidget->setCurrentIndex( 0 );
    mainTabWidget->setMovable( false );
    mainTabWidget->setTabsClosable( true );

    mainVSplitter = new QSplitter(Qt::Vertical, this);
    mainVSplitter->setContentsMargins(0, 0, 0, 0);
    mainVSplitter->setHandleWidth(3);
    mainVSplitter->setVisible(true);
    mainVSplitter->addWidget(textEdit1);
    mainVSplitter->addWidget(mainTabWidget);
    mainVSplitter->setSizes(QList<int>({200, 200}));

    mainGridLayout = new QGridLayout(this );
    mainGridLayout->setContentsMargins(0, 0, 0, 0);
    mainGridLayout->setVerticalSpacing(0);
    mainGridLayout->addLayout(topHLayout, 0, 0, 1, 1);
    mainGridLayout->addWidget(mainVSplitter, 1, 0, 1, 1);

    this->setLayout(mainGridLayout );
}

void AdaptixWidget::AddTab(QWidget *tab, QString title, QString icon) {
    if ( mainTabWidget->count() == 0 )
        mainVSplitter->setSizes(QList<int>() << 100 << 200);
    else if ( mainTabWidget->count() == 1 )
        mainTabWidget->setMovable(true);

    int id = mainTabWidget->addTab( tab, title );

    mainTabWidget->setIconSize( QSize( 17, 17 ) );
    mainTabWidget->setTabIcon(id, QIcon(icon));
    mainTabWidget->setCurrentIndex( id );
}

void AdaptixWidget::RemoveTab(int index) {
    if (index == -1)
        return;

    mainTabWidget->removeTab(index);

    if (mainTabWidget->count() == 0)
        mainVSplitter->setSizes(QList<int>() << 0);
    else if (mainTabWidget->count() == 1)
        mainTabWidget->setMovable(false);
}

void AdaptixWidget::LoadLogsUI() {
    if ( LogsTab == nullptr )
        LogsTab = new LogsWidget;

    this->AddTab(LogsTab, "Logs", ":/icons/logs");
}

void AdaptixWidget::ChannelClose() {
    LogInfo("WebSocker closed");
}

void AdaptixWidget::DataHandler(const QByteArray &data) {
    LogSuccess("Received data");
}

