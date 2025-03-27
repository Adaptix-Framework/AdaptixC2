#include <UI/Dialogs/DialogExtender.h>
#include <Utils/CustomElements.h>

DialogExtender::DialogExtender(Extender* e)
{
    extender = e;

    this->createUI();

    connect(buttonClose, &QPushButton::clicked, this, &DialogExtender::close);
    connect(tableWidget, &QTableWidget::customContextMenuRequested, this, &DialogExtender::handleMenu);
    connect(tableWidget, &QTableWidget::cellClicked, this, &DialogExtender::onRowSelect);
}

DialogExtender::~DialogExtender()  = default;

void DialogExtender::createUI()
{
    this->setWindowTitle("Extender");
    this->resize(1200, 600);

    tableWidget = new QTableWidget(this );
    tableWidget->setColumnCount(5);
    tableWidget->setContextMenuPolicy(Qt::CustomContextMenu );
    tableWidget->setAutoFillBackground(false );
    tableWidget->setShowGrid(false );
    tableWidget->setSortingEnabled(true );
    tableWidget->setWordWrap(true );
    tableWidget->setCornerButtonEnabled(false );
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows );
    tableWidget->setSelectionMode(QAbstractItemView::SingleSelection );
    tableWidget->setFocusPolicy(Qt::NoFocus );
    tableWidget->setAlternatingRowColors(true );
    tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch );
    tableWidget->horizontalHeader()->setCascadingSectionResizes(true );
    tableWidget->horizontalHeader()->setHighlightSections(false );
    tableWidget->verticalHeader()->setVisible(false );

    tableWidget->setHorizontalHeaderItem(0, new QTableWidgetItem("Name" ) );
    tableWidget->setHorizontalHeaderItem(1, new QTableWidgetItem("Path" ) );
    tableWidget->setHorizontalHeaderItem(2, new QTableWidgetItem("Description" ) );
    tableWidget->setHorizontalHeaderItem(3, new QTableWidgetItem("Status" ) );
    tableWidget->setHorizontalHeaderItem(4, new QTableWidgetItem("Comment" ) );

    tableWidget->setItemDelegate(new PaddingDelegate(tableWidget));
    tableWidget->hideColumn(4);

    textComment = new QTextEdit(this);
    textComment->setReadOnly( true );

    auto hSpacer1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto hSpacer2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    buttonClose = new QPushButton("Close", this);
    buttonClose->setFixedWidth(180);

    layout = new QGridLayout(this);
    layout->setContentsMargins( 4, 4,  4, 4);
    layout->addWidget(tableWidget, 0, 0, 1, 3);
    layout->addWidget(textComment, 1, 0, 1, 3);
    layout->addItem( hSpacer1, 2, 0, 1, 1);
    layout->addWidget( buttonClose, 2, 1, 1, 1);
    layout->addItem( hSpacer2, 2, 2, 1, 1);

    this->setLayout(layout);
}

void DialogExtender::AddExtenderItem(const ExtensionFile &extenderItem) const
{
    auto item_Name        = new QTableWidgetItem( extenderItem.Name );
    auto item_Path        = new QTableWidgetItem( extenderItem.FilePath );
    auto item_Description = new QTableWidgetItem( extenderItem.Description );
    auto item_Comment     = new QTableWidgetItem( extenderItem.Comment );
    auto item_Status      = new QTableWidgetItem( "" );

    item_Name->setFlags( item_Name->flags() ^ Qt::ItemIsEditable );

    item_Path->setFlags( item_Path->flags() ^ Qt::ItemIsEditable );

    item_Description->setFlags( item_Description->flags() ^ Qt::ItemIsEditable );

    item_Status->setFlags( item_Status->flags() ^ Qt::ItemIsEditable );
    item_Status->setTextAlignment( Qt::AlignCenter );
    if ( extenderItem.Enabled ) {
        item_Status->setText("Enable");
        item_Status->setForeground(QColor(COLOR_NeonGreen));
    }
    else {
        item_Status->setText("Disable");
        item_Status->setForeground(QColor(COLOR_ChiliPepper));
    }

    if( tableWidget->rowCount() < 1 )
        tableWidget->setRowCount( 1 );
    else
        tableWidget->setRowCount( tableWidget->rowCount() + 1 );

    bool isSortingEnabled = tableWidget->isSortingEnabled();
    tableWidget->setSortingEnabled( false );
    tableWidget->setItem( tableWidget->rowCount() - 1, 0, item_Name );
    tableWidget->setItem( tableWidget->rowCount() - 1, 1, item_Path );
    tableWidget->setItem( tableWidget->rowCount() - 1, 2, item_Description );
    tableWidget->setItem( tableWidget->rowCount() - 1, 3, item_Status );
    tableWidget->setItem( tableWidget->rowCount() - 1, 4, item_Comment );
    tableWidget->setSortingEnabled( isSortingEnabled );

    tableWidget->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 3, QHeaderView::ResizeToContents );
}

void DialogExtender::UpdateExtenderItem(const ExtensionFile &extenderItem) const
{
    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, 1);
        if ( item && item->text() == extenderItem.FilePath ) {
            tableWidget->item(row, 0)->setText(extenderItem.Name);
            tableWidget->item(row, 2)->setText(extenderItem.Description);
            tableWidget->item(row, 4)->setText(extenderItem.Comment);

            if ( extenderItem.Enabled ) {
                tableWidget->item(row, 3)->setText("Enable");
                tableWidget->item(row, 3)->setForeground(QColor(COLOR_NeonGreen));
            }
            else {
                tableWidget->item(row, 3)->setText("Disable");
                tableWidget->item(row, 3)->setForeground(QColor(COLOR_ChiliPepper));
            }

            break;
        }
    }
}

void DialogExtender::RemoveExtenderItem(const ExtensionFile &extenderItem) const
{
    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, 1);
        if ( item && item->text() == extenderItem.FilePath ) {
            tableWidget->removeRow(row);
            break;
        }
    }
}

/// SLOTS

void DialogExtender::handleMenu(const QPoint &pos ) const
{
    QMenu menu = QMenu();

    menu.addAction("Load new", this, &DialogExtender::onActionLoad );
    menu.addAction("Reload", this, &DialogExtender::onActionReload );
    menu.addSeparator();
    menu.addAction("Enable", this, &DialogExtender::onActionEnable );
    menu.addAction("Disable", this, &DialogExtender::onActionDisable );
    menu.addSeparator();
    menu.addAction("Remove", this, &DialogExtender::onActionRemove );

    QPoint globalPos = tableWidget->mapToGlobal(pos );
    menu.exec(globalPos );
}

void DialogExtender::onActionLoad() const
{
    QString filePath = QFileDialog::getOpenFileName( nullptr, "Select file", QDir::homePath(), "*.json");
    if ( filePath.isEmpty())
        return;

    extender->LoadFromFile(filePath, true);
}

void DialogExtender::onActionReload() const
{
    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( tableWidget->item(rowIndex, 1)->isSelected() ) {
            auto filePath = tableWidget->item( rowIndex, 1 )->text();
            extender->LoadFromFile(filePath, true);
        }
    }
}

void DialogExtender::onActionEnable() const
{
    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( tableWidget->item(rowIndex, 1)->isSelected() ) {
            auto filePath = tableWidget->item( rowIndex, 1 )->text();
            extender->EnableExtension(filePath);
        }
    }
}

void DialogExtender::onActionDisable() const
{
    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( tableWidget->item(rowIndex, 1)->isSelected() ) {
            auto filePath = tableWidget->item( rowIndex, 1 )->text();
            extender->DisableExtension(filePath);
        }
    }
}

void DialogExtender::onActionRemove() const
{
    QStringList FilesList;
    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( tableWidget->item(rowIndex, 1)->isSelected() ) {
            auto filePath = tableWidget->item( rowIndex, 1 )->text();
            FilesList.append(filePath);
        }
    }

    for(auto filePath : FilesList)
        extender->RemoveExtension(filePath);

    textComment->clear();
}

void DialogExtender::onRowSelect(int row, int column) const
{
    textComment->setText(tableWidget->item(row,4)->text());
}
