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
     tableWidget->setColumnCount( 11 );
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
     tableWidget->setHorizontalHeaderItem( 7, new QTableWidgetItem("Lport"));
     tableWidget->setHorizontalHeaderItem( 8, new QTableWidgetItem("Client"));
     tableWidget->setHorizontalHeaderItem( 9, new QTableWidgetItem("Rhost"));
     tableWidget->setHorizontalHeaderItem( 10, new QTableWidgetItem("Rport"));
     tableWidget->hideColumn( 0 );

     mainGridLayout = new QGridLayout( this );
     mainGridLayout->setContentsMargins( 0, 0,  0, 0);
     mainGridLayout->addWidget( tableWidget, 0, 0, 1, 1);
}

void TunnelsWidget::Clear()
{
     auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
     adaptixWidget->Tunnels.clear();
     for (int index = tableWidget->rowCount(); index > 0; index-- )
         tableWidget->removeRow(index -1 );
}

/// Slots

void TunnelsWidget::handleTunnelsMenu(const QPoint &pos ) const
{
     QMenu tunnelsMenu = QMenu();

     tunnelsMenu.addAction("Stop", this, &TunnelsWidget::stopTunnel );

     QPoint globalPos = tableWidget->mapToGlobal( pos );
     tunnelsMenu.exec(globalPos);
}

void TunnelsWidget::stopTunnel()
{

}
