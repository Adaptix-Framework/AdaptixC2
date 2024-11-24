#include <UI/Widgets/SessionsTableWidget.h>
#include <Utils/Convert.h>
#include <Client/Requestor.h>

SessionsTableWidget::SessionsTableWidget( QWidget* w )
{
    this->mainWidget = w;
    this->createUI();

    connect( tableWidget, &QTableWidget::doubleClicked, this, &SessionsTableWidget::handleTableDoubleClicked );
    connect( tableWidget, &QTableWidget::customContextMenuRequested, this, &SessionsTableWidget::handleSessionsTableMenu );
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
    mainGridLayout->setContentsMargins( 0, 0,  0, 0);
    mainGridLayout->addWidget( tableWidget, 0, 0, 1, 1);
}

void SessionsTableWidget::Clear()
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );

    for (auto agentId : adaptixWidget->Agents.keys()) {
        Agent* agent = adaptixWidget->Agents[agentId];
        adaptixWidget->Agents.remove(agentId);
        delete agent->Console;
        delete agent;
    }

    for (int index = tableWidget->rowCount(); index > 0; index-- )
        tableWidget->removeRow(index -1 );
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

void SessionsTableWidget::RemoveAgentItem(QString agentId)
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    Agent* agent = adaptixWidget->Agents[agentId];
    adaptixWidget->Agents.remove(agentId);
    delete agent->Console;
    delete agent;

    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if (agentId == tableWidget->item( rowIndex, ColumnAgentID )->text()) {
            tableWidget->removeRow(rowIndex);
            break;
        }
    }
}


/// SLOTS

void SessionsTableWidget::handleTableDoubleClicked(const QModelIndex &index)
{
    QString AgentId = tableWidget->item(index.row(),0)->text();

    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    adaptixWidget->LoadConsoleUI(AgentId);
}

void SessionsTableWidget::handleSessionsTableMenu(const QPoint &pos )
{
    if ( ! tableWidget->itemAt(pos) )
        return;

    auto sep1 = new QAction();
    sep1->setSeparator( true );

    auto ctxMenu = QMenu();
    ctxMenu.addAction( "Console", this, &SessionsTableWidget::actionConsoleOpen);
    ctxMenu.addAction( sep1 );
    ctxMenu.addAction( "Tag", this, &SessionsTableWidget::actionAgentTag);
    ctxMenu.addAction( "Hide", this, &SessionsTableWidget::actionAgentHide);
    ctxMenu.addAction( "Remove", this, &SessionsTableWidget::actionAgentRemove);

    ctxMenu.exec(tableWidget->horizontalHeader()->viewport()->mapToGlobal(pos));
}

void SessionsTableWidget::actionConsoleOpen()
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( tableWidget->item(rowIndex, 0)->isSelected() ) {
            auto agentId = tableWidget->item( rowIndex, ColumnAgentID )->text();
            adaptixWidget->LoadConsoleUI(agentId);
        }
    }
}

void SessionsTableWidget::actionAgentTag()
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );

    QString agentId = tableWidget->item( tableWidget->currentRow(), ColumnAgentID )->text();
    QString tag = tableWidget->item( tableWidget->currentRow(), ColumnTags )->text();

    bool inputOk;
    QString newTag = QInputDialog::getText(nullptr, "Set tags", "Tag for " + agentId, QLineEdit::Normal,tag, &inputOk);
    if (inputOk && !newTag.isEmpty()) {
        QString message = QString();
        bool ok = false;
        bool result = HttpReqAgentSetTag(agentId, newTag, *(adaptixWidget->GetProfile()), &message, &ok);
        if( !result ) {
            MessageError("JWT error");
            return;
        }
    }
}

void SessionsTableWidget::actionAgentHide()
{
    QList<QString> listId;

    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( tableWidget->item(rowIndex, 0)->isSelected() ) {
            auto agentId = tableWidget->item( rowIndex, ColumnAgentID )->text();
            listId.append(agentId);
        }
    }

    for (auto id : listId)
        this->RemoveAgentItem(id);
}

void SessionsTableWidget::actionAgentRemove()
{
    QList<QString> listId;

    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( tableWidget->item(rowIndex, 0)->isSelected() ) {
            auto agentId = tableWidget->item( rowIndex, ColumnAgentID )->text();
            listId.append(agentId);
        }
    }

    for (auto id : listId) {
        QString message = QString();
        bool ok = false;
        bool result = HttpReqAgentRemove(id, *(adaptixWidget->GetProfile()), &message, &ok);
        if( !result ) {
            MessageError("JWT error");
            return;
        }
    }
}