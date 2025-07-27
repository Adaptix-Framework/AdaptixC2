#include <UI/Dialogs/DialogExtender.h>
#include <Utils/CustomElements.h>
#include <Client/Extender.h>

DialogExtender::DialogExtender(Extender* e)
{
    extender = e;

    this->createUI();

    connect(tableWidget, &QTableWidget::customContextMenuRequested, this, &DialogExtender::handleMenu);
    connect(tableWidget, &QTableWidget::cellClicked, this, &DialogExtender::onRowSelect);
    connect(buttonClose, &QPushButton::clicked,      this, &DialogExtender::close);
}

DialogExtender::~DialogExtender()  = default;

void DialogExtender::createUI()
{
    this->setWindowTitle("AxScript manager");
    this->resize(1200, 700);

    tableWidget = new QTableWidget(this);
    tableWidget->setColumnCount(4);
    tableWidget->setContextMenuPolicy(Qt::CustomContextMenu );
    tableWidget->setAutoFillBackground(false);
    tableWidget->setShowGrid(false);
    tableWidget->setSortingEnabled(true);
    tableWidget->setWordWrap(true);
    tableWidget->setCornerButtonEnabled(false);
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows );
    tableWidget->setFocusPolicy(Qt::NoFocus );
    tableWidget->setAlternatingRowColors(true);
    tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableWidget->horizontalHeader()->setCascadingSectionResizes(true);
    tableWidget->horizontalHeader()->setHighlightSections(false);
    tableWidget->verticalHeader()->setVisible(false);

    tableWidget->setHorizontalHeaderItem(0, new QTableWidgetItem("Name" ) );
    tableWidget->setHorizontalHeaderItem(1, new QTableWidgetItem("Path" ) );
    tableWidget->setHorizontalHeaderItem(2, new QTableWidgetItem("Status" ) );
    tableWidget->setHorizontalHeaderItem(3, new QTableWidgetItem("Description" ) );

    tableWidget->hideColumn(3);

    textComment = new QTextEdit(this);
    textComment->setReadOnly(true);

    splitter = new QSplitter(Qt::Vertical, this);
    splitter->setContentsMargins(0, 0, 0, 0);
    splitter->setHandleWidth(3);
    splitter->setVisible(true);
    splitter->addWidget(tableWidget);
    splitter->addWidget(textComment);
    splitter->setSizes(QList<int>({500, 140}));

    spacer1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    spacer2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    buttonClose = new QPushButton("Close", this);
    buttonClose->setProperty("ButtonStyle", "dialog");
    buttonClose->setFixedWidth(180);

    layout = new QGridLayout(this);
    layout->setContentsMargins( 4, 4,  4, 4);
    layout->addWidget(splitter,    0, 0, 1, 3);
    layout->addItem(  spacer1,     1, 0, 1, 1);
    layout->addWidget(buttonClose, 1, 1, 1, 1);
    layout->addItem(  spacer2,     1, 2, 1, 1);

    this->setLayout(layout);
}

void DialogExtender::AddExtenderItem(const ExtensionFile &extenderItem) const
{
    auto item_Name   = new QTableWidgetItem(extenderItem.Name);
    auto item_Path   = new QTableWidgetItem(extenderItem.FilePath);
    auto item_Desc   = new QTableWidgetItem(extenderItem.Description);
    auto item_Status = new QTableWidgetItem("");

    item_Name->setFlags( item_Name->flags() ^ Qt::ItemIsEditable );

    item_Path->setFlags( item_Path->flags() ^ Qt::ItemIsEditable );

    item_Status->setFlags( item_Status->flags() ^ Qt::ItemIsEditable );
    item_Status->setTextAlignment( Qt::AlignCenter );
    if ( extenderItem.Enabled ) {
        item_Status->setText("Enable");
        item_Status->setForeground(QColor(COLOR_NeonGreen));
    }
    else {
        if (extenderItem.Message.isEmpty()) {
            item_Status->setText("Disable");
            item_Status->setForeground(QColor(COLOR_BrightOrange));
        }
        else {
            item_Status->setText("Failed");
            item_Status->setForeground(QColor(COLOR_ChiliPepper));
        }
    }

    if( tableWidget->rowCount() < 1 )
        tableWidget->setRowCount( 1 );
    else
        tableWidget->setRowCount( tableWidget->rowCount() + 1 );

    bool isSortingEnabled = tableWidget->isSortingEnabled();
    tableWidget->setSortingEnabled( false );
    tableWidget->setItem( tableWidget->rowCount() - 1, 0, item_Name );
    tableWidget->setItem( tableWidget->rowCount() - 1, 1, item_Path );
    tableWidget->setItem( tableWidget->rowCount() - 1, 2, item_Status );
    tableWidget->setItem( tableWidget->rowCount() - 1, 3, item_Desc );
    tableWidget->setSortingEnabled( isSortingEnabled );

    tableWidget->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
}

void DialogExtender::UpdateExtenderItem(const ExtensionFile &extenderItem) const
{
    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, 1);
        if ( item && item->text() == extenderItem.FilePath ) {
            tableWidget->item(row, 0)->setText(extenderItem.Name);
            tableWidget->item(row, 3)->setText(extenderItem.Description);

            if ( extenderItem.Enabled ) {
                tableWidget->item(row, 2)->setText("Enable");
                tableWidget->item(row, 2)->setForeground(QColor(COLOR_NeonGreen));
            }
            else {
                if (extenderItem.Message.isEmpty()) {
                    tableWidget->item(row, 2)->setText("Disable");
                    tableWidget->item(row, 2)->setForeground(QColor(COLOR_BrightOrange));
                }
                else {
                    tableWidget->item(row, 2)->setText("Failed");
                    tableWidget->item(row, 2)->setForeground(QColor(COLOR_ChiliPepper));
                }
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
    menu.addAction("Reload",   this, &DialogExtender::onActionReload );
    menu.addSeparator();
    menu.addAction("Enable",  this, &DialogExtender::onActionEnable );
    menu.addAction("Disable", this, &DialogExtender::onActionDisable );
    menu.addSeparator();
    menu.addAction("Remove", this, &DialogExtender::onActionRemove );

    QPoint globalPos = tableWidget->mapToGlobal(pos);
    menu.exec(globalPos);
}

void DialogExtender::onActionLoad() const
{
    QString filePath = QFileDialog::getOpenFileName(nullptr, "Load Script", "", "AxScript Files (*.axs)");
    if ( filePath.isEmpty())
        return;

    extender->LoadFromFile(filePath, true);
}

void DialogExtender::onActionReload() const
{
    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( tableWidget->item(rowIndex, 1)->isSelected() ) {
            auto filePath = tableWidget->item( rowIndex, 1 )->text();
            extender->RemoveExtension(filePath);
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

void DialogExtender::onRowSelect(const int row, int column) const { textComment->setText(tableWidget->item(row,3)->text()); }
