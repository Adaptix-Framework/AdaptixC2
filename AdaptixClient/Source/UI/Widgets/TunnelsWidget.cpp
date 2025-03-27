#include <UI/Widgets/TunnelsWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/Requestor.h>

TunnelsWidget::TunnelsWidget(QWidget* w)
{
     this->mainWidget = w;

     this->createUI();

     connect( tableWidget, &QTableWidget::customContextMenuRequested, this, &TunnelsWidget::handleTunnelsMenu );
}

TunnelsWidget::~TunnelsWidget() = default;

void TunnelsWidget::createUI()
{
     tableWidget = new QTableWidget( this );
     tableWidget->setColumnCount( 12 );
     tableWidget->setContextMenuPolicy( Qt::CustomContextMenu );
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

     tableWidget->setHorizontalHeaderItem( 0, new QTableWidgetItem("Tunnel ID"));
     tableWidget->setHorizontalHeaderItem( 1, new QTableWidgetItem("Agent ID"));
     tableWidget->setHorizontalHeaderItem( 2, new QTableWidgetItem("Computer"));
     tableWidget->setHorizontalHeaderItem( 3, new QTableWidgetItem("User"));
     tableWidget->setHorizontalHeaderItem( 4, new QTableWidgetItem("Process"));
     tableWidget->setHorizontalHeaderItem( 5, new QTableWidgetItem("Type"));
     tableWidget->setHorizontalHeaderItem( 6, new QTableWidgetItem("Info"));
     tableWidget->setHorizontalHeaderItem( 7, new QTableWidgetItem("Interface"));
     tableWidget->setHorizontalHeaderItem( 8, new QTableWidgetItem("Listen port"));
     tableWidget->setHorizontalHeaderItem( 9, new QTableWidgetItem("Client"));
     tableWidget->setHorizontalHeaderItem( 10, new QTableWidgetItem("Forward host"));
     tableWidget->setHorizontalHeaderItem( 11, new QTableWidgetItem("Forward port"));
     tableWidget->hideColumn( 0 );

     mainGridLayout = new QGridLayout( this );
     mainGridLayout->setContentsMargins( 0, 0,  0, 0);
     mainGridLayout->addWidget( tableWidget, 0, 0, 1, 1);
}

void TunnelsWidget::Clear() const
{
     auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
     adaptixWidget->Tunnels.clear();
     for (int index = tableWidget->rowCount(); index > 0; index-- )
         tableWidget->removeRow(index -1 );
}

void TunnelsWidget::AddTunnelItem(TunnelData newTunnel) const
{
     auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
     for( auto tunnel : adaptixWidget->Tunnels ) {
          if( tunnel.TunnelId == newTunnel.TunnelId )
               return;
     }

     auto item_TunnelId  = new QTableWidgetItem( newTunnel.TunnelId );
     auto item_AgentId   = new QTableWidgetItem( newTunnel.AgentId );
     auto item_Computer  = new QTableWidgetItem( newTunnel.Computer );
     auto item_User      = new QTableWidgetItem( newTunnel.Username );
     auto item_Process   = new QTableWidgetItem( newTunnel.Process );
     auto item_Type      = new QTableWidgetItem( newTunnel.Type );
     auto item_Info      = new QTableWidgetItem( newTunnel.Info );
     auto item_Interface = new QTableWidgetItem( newTunnel.Interface );
     auto item_Port      = new QTableWidgetItem( newTunnel.Port );
     auto item_Client    = new QTableWidgetItem( newTunnel.Client );
     auto item_Fhost     = new QTableWidgetItem( newTunnel.Fhost );
     auto item_Fport     = new QTableWidgetItem( newTunnel.Fport );

     item_TunnelId->setFlags( item_TunnelId->flags() ^ Qt::ItemIsEditable );
     item_TunnelId->setTextAlignment( Qt::AlignCenter );

     item_AgentId->setFlags( item_AgentId->flags() ^ Qt::ItemIsEditable );
     item_AgentId->setTextAlignment( Qt::AlignCenter );

     item_Computer->setFlags( item_Computer->flags() ^ Qt::ItemIsEditable );
     item_Computer->setTextAlignment( Qt::AlignCenter );

     item_User->setFlags( item_User->flags() ^ Qt::ItemIsEditable );
     item_User->setTextAlignment( Qt::AlignCenter );

     item_Process->setFlags( item_Process->flags() ^ Qt::ItemIsEditable );
     item_Process->setTextAlignment( Qt::AlignCenter );

     item_Type->setFlags( item_Type->flags() ^ Qt::ItemIsEditable );
     item_Type->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );

     item_Info->setFlags( item_Info->flags() ^ Qt::ItemIsEditable );
     item_Info->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );

     item_Interface->setFlags( item_Interface->flags() ^ Qt::ItemIsEditable );
     item_Interface->setTextAlignment( Qt::AlignCenter );

     item_Port->setFlags( item_Port->flags() ^ Qt::ItemIsEditable );
     item_Port->setTextAlignment( Qt::AlignCenter );

     item_Client->setFlags( item_Client->flags() ^ Qt::ItemIsEditable );
     item_Client->setTextAlignment( Qt::AlignCenter );

     item_Fhost->setFlags( item_Fhost->flags() ^ Qt::ItemIsEditable );
     item_Fhost->setTextAlignment( Qt::AlignCenter );

     item_Fport->setFlags( item_Fport->flags() ^ Qt::ItemIsEditable );
     item_Fport->setTextAlignment( Qt::AlignCenter );

     if( tableWidget->rowCount() < 1 )
          tableWidget->setRowCount( 1 );
     else
          tableWidget->setRowCount( tableWidget->rowCount() + 1 );

     bool isSortingEnabled = tableWidget->isSortingEnabled();
     tableWidget->setSortingEnabled( false );
     tableWidget->setItem( tableWidget->rowCount() - 1, 0, item_TunnelId );
     tableWidget->setItem( tableWidget->rowCount() - 1, 1, item_AgentId );
     tableWidget->setItem( tableWidget->rowCount() - 1, 2, item_Computer );
     tableWidget->setItem( tableWidget->rowCount() - 1, 3, item_User );
     tableWidget->setItem( tableWidget->rowCount() - 1, 4, item_Process );
     tableWidget->setItem( tableWidget->rowCount() - 1, 5, item_Type );
     tableWidget->setItem( tableWidget->rowCount() - 1, 6, item_Info );
     tableWidget->setItem( tableWidget->rowCount() - 1, 7, item_Interface );
     tableWidget->setItem( tableWidget->rowCount() - 1, 8, item_Port );
     tableWidget->setItem( tableWidget->rowCount() - 1, 9, item_Client );
     tableWidget->setItem( tableWidget->rowCount() - 1, 10, item_Fhost );
     tableWidget->setItem( tableWidget->rowCount() - 1, 11, item_Fport );

     tableWidget->setSortingEnabled( isSortingEnabled );

    tableWidget->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 7, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 8, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 10, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 11, QHeaderView::ResizeToContents );

    adaptixWidget->Tunnels.push_back(newTunnel);
}

void TunnelsWidget::EditTunnelItem(const QString &tunnelId, const QString &info) const
{
     auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
     for ( int i = 0; i < adaptixWidget->Tunnels.size(); i++ ) {
          if( adaptixWidget->Tunnels[i].TunnelId == tunnelId ) {
               adaptixWidget->Tunnels[i].Info = info;
               break;
          }
     }

     for ( int row = 0; row < tableWidget->rowCount(); ++row ) {
          QTableWidgetItem *item = tableWidget->item(row, 0);
          if ( item && item->text() == tunnelId ) {
               tableWidget->item(row, 6)->setText(info);
               break;
          }
     }
}

void TunnelsWidget::RemoveTunnelItem(const QString &tunnelId) const
{
     auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
     for ( int i = 0; i < adaptixWidget->Tunnels.size(); i++ ) {
          if( adaptixWidget->Tunnels[i].TunnelId == tunnelId ) {
               adaptixWidget->Tunnels.erase( adaptixWidget->Tunnels.begin() + i );
               break;
          }
     }

     for ( int row = 0; row < tableWidget->rowCount(); ++row ) {
          QTableWidgetItem *item = tableWidget->item(row, 0);
          if ( item && item->text() == tunnelId ) {
               tableWidget->removeRow(row);
               break;
         }
     }
}

/// Slots

void TunnelsWidget::handleTunnelsMenu(const QPoint &pos ) const
{
     QMenu tunnelsMenu = QMenu();

     tunnelsMenu.addAction( "Set info", this, &TunnelsWidget::actionSetInfo);
     tunnelsMenu.addAction("Stop", this, &TunnelsWidget::actionStopTunnel );

     QPoint globalPos = tableWidget->mapToGlobal( pos );
     tunnelsMenu.exec(globalPos);
}

void TunnelsWidget::actionSetInfo() const
{
     if (tableWidget->selectionModel()->selectedRows().empty())
          return;

     QString tunnelId = tableWidget->item( tableWidget->currentRow(), 0 )->text();
     QString info     = tableWidget->item( tableWidget->currentRow(), 6 )->text();
     auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
     if ( !adaptixWidget )
          return;

     bool inputOk;
     QString newInfo = QInputDialog::getText(nullptr, "Set info", "Info:", QLineEdit::Normal,info, &inputOk);
     if ( inputOk ) {
          QString message = QString();
          bool ok = false;
          bool result = HttpReqTunnelSetInfo(tunnelId, newInfo, *(adaptixWidget->GetProfile()), &message, &ok);
          if( !result ) {
               MessageError("JWT error");
               return;
          }
     }
}

void TunnelsWidget::actionStopTunnel() const
{
     if (tableWidget->selectionModel()->selectedRows().empty())
          return;

     auto tunnelId = tableWidget->item( tableWidget->currentRow(), 0 )->text();
     auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
     if ( !adaptixWidget )
          return;

     QString message = QString();
     bool ok = false;
     bool result = HttpReqTunnelStop( tunnelId, *(adaptixWidget->GetProfile()), &message, &ok );
     if( !result ){
          MessageError("JWT error");
          return;
     }

     if ( !ok ) {
          MessageError(message);
     }
}
