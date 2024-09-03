#include <UI/Widgets/AdaptixWidget.h>

AdaptixWidget::AdaptixWidget() {
    this->createUI();

    QTextEdit *textEdit2 = new QTextEdit(this);
    this->AddNewTab(textEdit2, "TestTab");
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


    topLayout = new QHBoxLayout;
    topLayout->setObjectName( QString::fromUtf8( "TopLayout" ) );
    topLayout->setContentsMargins( 5, 5, 0, 5);
    topLayout->setSpacing(10);
    topLayout->setAlignment(Qt::AlignLeft);

    topLayout->addWidget(listenersButton);
    topLayout->addWidget(logsButton);
    topLayout->addWidget(line_1);
    topLayout->addWidget(sessionsButton);
    topLayout->addWidget(graphButton);
    topLayout->addWidget(targetsButton);
    topLayout->addWidget(line_2);
    topLayout->addWidget(jobsButton);
    topLayout->addWidget(proxyButton);
    topLayout->addWidget(line_3);
    topLayout->addWidget(downloadsButton);
    topLayout->addWidget(credsButton);
    topLayout->addWidget(screensButton);
    topLayout->addWidget(keysButton);
    topLayout->addWidget(line_4);
    topLayout->addWidget(reconnectButton);
    topLayout->addItem(horizontalSpacer1);


    /// TODO:
    QTextEdit *textEdit1 = new QTextEdit(this);
    ///

    mainTabWidget = new QTabWidget(this);
    mainTabWidget->setObjectName( QString::fromUtf8( "MainTabWidget" ) );
    mainTabWidget->setCurrentIndex( 0 );
    mainTabWidget->setMovable( false );

    mainVSplitter = new QSplitter(Qt::Vertical, this);
    mainVSplitter->setContentsMargins(0, 0, 0, 0);
    mainVSplitter->setHandleWidth(3);
    mainVSplitter->setVisible(true);
    mainVSplitter->addWidget(textEdit1);
    mainVSplitter->addWidget(mainTabWidget);
    mainVSplitter->setSizes(QList<int>({200, 200}));


    gridLayout_Main = new QGridLayout( this );
    gridLayout_Main->setObjectName( QString::fromUtf8("MainGridLayout" ) );
    gridLayout_Main->setContentsMargins(0, 0, 0, 0);
    gridLayout_Main->setVerticalSpacing(0);
    gridLayout_Main->addLayout( topLayout, 0, 0, 1, 1);
    gridLayout_Main->addWidget( mainVSplitter, 1, 0, 1, 1);

    this->setLayout( gridLayout_Main );
}

void AdaptixWidget::AddNewTab(QWidget *tab, QString title, QString icon) {
    int id = 0;
    if ( mainTabWidget->count() == 0 ) {
        mainVSplitter->setSizes(QList<int>() << 100 << 200);
    }
    else if ( mainTabWidget->count() == 1 ) {
        mainTabWidget->setMovable(true);
    }

    mainTabWidget->setTabsClosable( true );

    id = mainTabWidget->addTab( tab, title );

    mainTabWidget->setIconSize( QSize( 17, 17 ) );
    mainTabWidget->setTabIcon(id, QIcon(icon));
    mainTabWidget->setCurrentIndex( id );
}
