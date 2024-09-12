#include <UI/Widgets/ListenersWidget.h>

ListenersWidget::ListenersWidget() {
    this->createUI();

    connect( tableWidget, &QTableWidget::customContextMenuRequested, this, &ListenersWidget::handleListenersMenu );
}

ListenersWidget::~ListenersWidget() = default;

void ListenersWidget::createUI() {

    if ( this->objectName().isEmpty() )
        this->setObjectName( QString::fromUtf8( "ListenersWidget" ) );

    menuListeners = new QMenu( this );
    menuListeners->setObjectName( QString::fromUtf8( "ListenersMenuListeners" ) );
    menuListeners->addAction( "Create", this, &ListenersWidget::CreateListener );

    tableWidget = new QTableWidget( this );

    tableWidget->setColumnCount( 6 );
    tableWidget->setObjectName( QString::fromUtf8( "ListenersTableListeners" ) );
    tableWidget->setContextMenuPolicy( Qt::CustomContextMenu );
    tableWidget->addAction( menuListeners->menuAction() );
    tableWidget->setAutoFillBackground( false );
    tableWidget->setShowGrid( false );
    tableWidget->setSortingEnabled( true );
    tableWidget->setWordWrap( true );
    tableWidget->setCornerButtonEnabled( false );
    tableWidget->setSelectionBehavior( QAbstractItemView::SelectRows );
    tableWidget->setSelectionMode( QAbstractItemView::SingleSelection );
    tableWidget->setFocusPolicy( Qt::NoFocus );
    tableWidget->setAlternatingRowColors( true );
    tableWidget->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    tableWidget->horizontalHeader()->setCascadingSectionResizes( true );
    tableWidget->horizontalHeader()->setHighlightSections( false );
    tableWidget->verticalHeader()->setVisible( false );
    tableWidget->verticalHeader()->setDefaultSectionSize( 12 );

    tableWidget->setHorizontalHeaderItem( 0, new QTableWidgetItem( "Name" ) );
    tableWidget->setHorizontalHeaderItem( 1, new QTableWidgetItem( "Type" ) );
    tableWidget->setHorizontalHeaderItem( 2, new QTableWidgetItem( "Interface" ) );
    tableWidget->setHorizontalHeaderItem( 3, new QTableWidgetItem( "Port" ) );
    tableWidget->setHorizontalHeaderItem( 4, new QTableWidgetItem( "C2 Hosts" ) );
    tableWidget->setHorizontalHeaderItem( 5, new QTableWidgetItem( "Status" ) );

    tableWidget->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 3, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 5, QHeaderView::ResizeToContents );

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->setObjectName( QString::fromUtf8( "MainLayoutListeners" ) );
    mainGridLayout->setContentsMargins( 1, 3,  1, 3);
    mainGridLayout->addWidget( tableWidget, 0, 0, 1, 1);
}

void ListenersWidget::handleListenersMenu(const QPoint &pos ) const {
    QPoint globalPos = tableWidget->mapToGlobal( pos );
    menuListeners->exec( globalPos );
}

void ListenersWidget::CreateListener() {
    dialogListener = new DialogListener;
    dialogListener->exec();
}

