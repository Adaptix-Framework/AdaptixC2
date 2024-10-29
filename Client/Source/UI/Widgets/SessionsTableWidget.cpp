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
    if ( this->objectName().isEmpty() )
        this->setObjectName( QString::fromUtf8( "SessionsTableWidget" ) );

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

void SessionsTableWidget::AddAgentItem(AgentData newAgent )
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    for( auto agent : adaptixWidget->Agents ) {
        if( agent.Id == newAgent.Id ) {
            return;
        }
    }

    auto username = newAgent.Username;
    if (newAgent.Elevated)
        username = "* " + username;

    auto sleep = QString("%1 (%2%)").arg( FormatSecToStr(newAgent.Sleep) ).arg(newAgent.Jitter);

    auto item_Id       = new QTableWidgetItem( newAgent.Id );
    auto item_Type     = new QTableWidgetItem( newAgent.Name );
    auto item_Listener = new QTableWidgetItem( newAgent.Listener );
    auto item_External = new QTableWidgetItem( newAgent.ExternalIP );
    auto item_Internal = new QTableWidgetItem( newAgent.InternalIP );
    auto item_Domain   = new QTableWidgetItem( newAgent.Domain );
    auto item_Computer = new QTableWidgetItem( newAgent.Computer );
    auto item_Username = new QTableWidgetItem( username );
    auto item_Os       = new QTableWidgetItem( newAgent.OsDesc );
    auto item_Process  = new QTableWidgetItem( newAgent.Process );
    auto item_Pid      = new QTableWidgetItem( newAgent.Pid );
    auto item_Tid      = new QTableWidgetItem( newAgent.Tid );
    auto item_Tags     = new QTableWidgetItem(  );
    auto item_Last     = new QTableWidgetItem(  );
    auto item_Sleep    = new QTableWidgetItem( sleep );

    item_Id->setFlags( item_Id->flags() ^ Qt::ItemIsEditable );
    item_Id->setTextAlignment( Qt::AlignCenter );

    item_Type->setFlags( item_Type->flags() ^ Qt::ItemIsEditable );
    item_Type->setTextAlignment( Qt::AlignCenter );

    item_Listener->setFlags( item_Listener->flags() ^ Qt::ItemIsEditable );
    item_Listener->setTextAlignment( Qt::AlignCenter );

    item_External->setFlags( item_External->flags() ^ Qt::ItemIsEditable );
    item_External->setTextAlignment( Qt::AlignCenter );

    item_Internal->setFlags( item_Internal->flags() ^ Qt::ItemIsEditable );
    item_Internal->setTextAlignment( Qt::AlignCenter );

    item_Domain->setFlags( item_Domain->flags() ^ Qt::ItemIsEditable );
    item_Domain->setTextAlignment( Qt::AlignCenter );

    item_Computer->setFlags( item_Computer->flags() ^ Qt::ItemIsEditable );
    item_Computer->setTextAlignment( Qt::AlignCenter );

    item_Username->setFlags( item_Username->flags() ^ Qt::ItemIsEditable );
    item_Username->setTextAlignment( Qt::AlignCenter );

    item_Os->setFlags( item_Os->flags() ^ Qt::ItemIsEditable );
    item_Os->setTextAlignment( Qt::AlignCenter );

    item_Process->setFlags( item_Process->flags() ^ Qt::ItemIsEditable );
    item_Process->setTextAlignment( Qt::AlignCenter );

    item_Pid->setFlags( item_Pid->flags() ^ Qt::ItemIsEditable );
    item_Pid->setTextAlignment( Qt::AlignCenter );

    item_Tid->setFlags( item_Tid->flags() ^ Qt::ItemIsEditable );
    item_Tid->setTextAlignment( Qt::AlignCenter );

    item_Tags->setFlags( item_Tags->flags() ^ Qt::ItemIsEditable );
    item_Tags->setTextAlignment( Qt::AlignCenter );

    item_Last->setFlags( item_Last->flags() ^ Qt::ItemIsEditable );
    item_Last->setTextAlignment( Qt::AlignCenter );

    item_Sleep->setFlags( item_Sleep->flags() ^ Qt::ItemIsEditable );
    item_Sleep->setTextAlignment( Qt::AlignCenter );

    if( tableWidget->rowCount() < 1 )
        tableWidget->setRowCount( 1 );
    else
        tableWidget->setRowCount( tableWidget->rowCount() + 1 );

    bool isSortingEnabled = tableWidget->isSortingEnabled();
    tableWidget->setSortingEnabled( false );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnAgentID,   item_Id );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnAgentType, item_Type );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnListener,  item_Listener );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnExternal,  item_External );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnInternal,  item_Internal );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnDomain,    item_Domain );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnComputer,  item_Computer );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnUser,      item_Username );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnOs,        item_Os );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnProcess,   item_Process );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnProcessId, item_Pid );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnThreadId,  item_Tid );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnTags,      item_Tags );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnLast,      item_Last );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnSleep,     item_Sleep );
    tableWidget->setSortingEnabled( isSortingEnabled );

    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnAgentID, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnAgentType, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnListener, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnExternal, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnInternal, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnProcessId, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnThreadId, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnSleep, QHeaderView::ResizeToContents );

    adaptixWidget->Agents.push_back(newAgent);
}