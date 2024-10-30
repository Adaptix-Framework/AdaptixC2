#include <UI/Widgets/SessionsTableWidget.h>
#include <Utils/Convert.h>

SessionsTableWidget::SessionsTableWidget( QWidget* w )
{
    this->mainWidget = w;
    this->createUI();
}

SessionsTableWidget::~SessionsTableWidget() = default;

void SessionsTableWidget::createUI()
{
    titleAgentID   = new QTableWidgetItem( "Agent ID" );
    titleAgentType = new QTableWidgetItem( "Agent Type" );
    titleListener  = new QTableWidgetItem( "Listener" );
    titleExternal  = new QTableWidgetItem( "External" );
    titleInternal  = new QTableWidgetItem( "Internal" );
    titleDomain    = new QTableWidgetItem( "Domain" );
    titleComputer  = new QTableWidgetItem( "Computer" );
    titleUser      = new QTableWidgetItem( "User" );
    titleOs        = new QTableWidgetItem( "OS" );
    titleProcess   = new QTableWidgetItem( "Process" );
    titleProcessId = new QTableWidgetItem( "PID" );
    titleThreadId  = new QTableWidgetItem( "TID" );
    titleTag       = new QTableWidgetItem( "Tags" );
    titleLast      = new QTableWidgetItem( "Last" );
    titleSleep     = new QTableWidgetItem( "Sleep" );

    tableWidget = new QTableWidget( this );
    tableWidget->setColumnCount( ColumnCount );
    tableWidget->setContextMenuPolicy( Qt::CustomContextMenu );
    tableWidget->setAutoFillBackground( false );
    tableWidget->setShowGrid( false );
    tableWidget->setSortingEnabled( true );
    tableWidget->setWordWrap( true );
    tableWidget->setCornerButtonEnabled( false );
    tableWidget->setSelectionBehavior( QAbstractItemView::SelectRows );
    tableWidget->setFocusPolicy( Qt::NoFocus );
    tableWidget->setAlternatingRowColors( true );
    tableWidget->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    tableWidget->horizontalHeader()->setCascadingSectionResizes( true );
    tableWidget->horizontalHeader()->setHighlightSections( false );
    tableWidget->verticalHeader()->setVisible( false );

    tableWidget->setHorizontalHeaderItem( ColumnAgentID,   titleAgentID );
    tableWidget->setHorizontalHeaderItem( ColumnAgentType, titleAgentType );
    tableWidget->setHorizontalHeaderItem( ColumnListener,  titleListener );
    tableWidget->setHorizontalHeaderItem( ColumnExternal,  titleExternal );
    tableWidget->setHorizontalHeaderItem( ColumnInternal,  titleInternal );
    tableWidget->setHorizontalHeaderItem( ColumnDomain,    titleDomain );
    tableWidget->setHorizontalHeaderItem( ColumnComputer,  titleComputer );
    tableWidget->setHorizontalHeaderItem( ColumnUser,      titleUser );
    tableWidget->setHorizontalHeaderItem( ColumnOs,        titleOs );
    tableWidget->setHorizontalHeaderItem( ColumnProcess,   titleProcess );
    tableWidget->setHorizontalHeaderItem( ColumnProcessId, titleProcessId );
    tableWidget->setHorizontalHeaderItem( ColumnThreadId,  titleThreadId );
    tableWidget->setHorizontalHeaderItem( ColumnTags,      titleTag );
    tableWidget->setHorizontalHeaderItem( ColumnLast,      titleLast );
    tableWidget->setHorizontalHeaderItem( ColumnSleep,     titleSleep );

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->setObjectName( QString::fromUtf8( "MainLayoutSessionsTable" ) );
    mainGridLayout->setContentsMargins( 0, 0,  0, 0);
    mainGridLayout->addWidget( tableWidget, 0, 0, 1, 1);
}

void SessionsTableWidget::AddAgentItem( Agent* newAgent )
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );

    if ( adaptixWidget->Agents.contains(newAgent->data.Id) )
        return;

    adaptixWidget->Agents[ newAgent->data.Id ] = newAgent;

    if( tableWidget->rowCount() < 1 )
        tableWidget->setRowCount( 1 );
    else
        tableWidget->setRowCount( tableWidget->rowCount() + 1 );

    bool isSortingEnabled = tableWidget->isSortingEnabled();
    tableWidget->setSortingEnabled( false );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnAgentID,   newAgent->item_Id );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnAgentType, newAgent->item_Type );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnListener,  newAgent->item_Listener );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnExternal,  newAgent->item_External );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnInternal,  newAgent->item_Internal );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnDomain,    newAgent->item_Domain );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnComputer,  newAgent->item_Computer );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnUser,      newAgent->item_Username );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnOs,        newAgent->item_Os );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnProcess,   newAgent->item_Process );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnProcessId, newAgent->item_Pid );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnThreadId,  newAgent->item_Tid );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnTags,      newAgent->item_Tags );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnLast,      newAgent->item_Last );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnSleep,     newAgent->item_Sleep );
    tableWidget->setSortingEnabled( isSortingEnabled );

    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnAgentID,   QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnAgentType, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnListener,  QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnExternal,  QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnInternal,  QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnProcessId, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnThreadId,  QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnSleep,     QHeaderView::ResizeToContents );
}