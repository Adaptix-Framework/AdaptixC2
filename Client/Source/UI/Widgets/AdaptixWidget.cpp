#include <UI/Widgets/AdaptixWidget.h>

AdaptixWidget::AdaptixWidget() {

    mainWidget = new QWidget( this );
    mainWidget->setObjectName( QString::fromUtf8( "MainWidget" ) );

    gridLayout_Main = new QGridLayout( mainWidget );
    gridLayout_Main->setObjectName( QString::fromUtf8("gridLayout_Main" ) );

    topWidget = new QWidget(mainWidget);
    topWidget->setMaximumSize(16777215, 34);
    topWidget->setContentsMargins(5, 0, 0, 0);

    topLayout = new QHBoxLayout(topWidget);
    topLayout->setContentsMargins( 5, 5, 0, 5);
    topLayout->setSpacing(9);
    topLayout->setAlignment(Qt::AlignLeft);

    listenersButton = new QPushButton( QIcon(":/icons/listeners"), "", topWidget );
    listenersButton->setObjectName( QString::fromUtf8( "ListenersMenuButton" ) );
    listenersButton->setIconSize( QSize( 24,24 ));
    listenersButton->setMinimumSize( 37,28 );
    listenersButton->setMaximumSize( 37,28 );
    listenersButton->setToolTip("Listeners & Sites");

    sessionsButton = new QPushButton( QIcon(":/icons/format_list"), "", topWidget );
    sessionsButton->setObjectName( QString::fromUtf8( "SessionsMenuButton" ) );
    sessionsButton->setIconSize( QSize( 24,24 ));
    sessionsButton->setMinimumSize( 37, 28 );
    sessionsButton->setMaximumSize( 37, 28 );
    sessionsButton->setToolTip("Sessions table");

    logsButton = new QPushButton(QIcon(":/icons/logs"), "", topWidget);
    logsButton->setObjectName( QString::fromUtf8( "logsButton" ) );
    logsButton->setIconSize( QSize( 24,24 ));
    logsButton->setMinimumSize( 35,28 );
    logsButton->setMaximumSize( 35,28 );
    logsButton->setToolTip("Logs");

    topLayout->addWidget(listenersButton);
    topLayout->addWidget(sessionsButton);
    topLayout->addWidget(logsButton);

    gridLayout_Main->addWidget( topWidget, 0, 0, 1, 1 );
    gridLayout_Main->setContentsMargins( 0, 0, 0, 0 );
}