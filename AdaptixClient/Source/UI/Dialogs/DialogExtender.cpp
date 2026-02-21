#include <UI/Dialogs/DialogExtender.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Utils/CustomElements.h>
#include <Utils/NonBlockingDialogs.h>
#include <Client/Extender.h>
#include <Client/AuthProfile.h>
#include <UI/MainUI.h>
#include <MainAdaptix.h>

DialogExtender::DialogExtender(Extender* e)
{
    extender = e;

    this->createUI();

    connect(tableWidget,        &QTableWidget::customContextMenuRequested, this, &DialogExtender::handleMenu);
    connect(tableWidget,        &QTableWidget::cellClicked,                this, &DialogExtender::onRowSelect);
    connect(serverTableWidget,  &QTableWidget::customContextMenuRequested, this, &DialogExtender::handleServerMenu);
    connect(serverTableWidget,  &QTableWidget::cellClicked,                this, &DialogExtender::onServerRowSelect);
    connect(serverProjectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DialogExtender::onProjectChanged);
    connect(buttonClose,        &QPushButton::clicked,                     this, &DialogExtender::close);
}

DialogExtender::~DialogExtender()  = default;

static QTableWidget* createScriptTable(QWidget* parent, int columnCount)
{
    auto* table = new QTableWidget(parent);
    table->setColumnCount(columnCount);
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    table->setAutoFillBackground(false);
    table->setShowGrid(false);
    table->setSortingEnabled(true);
    table->setWordWrap(true);
    table->setCornerButtonEnabled(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setFocusPolicy(Qt::NoFocus);
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setCascadingSectionResizes(true);
    table->horizontalHeader()->setHighlightSections(false);
    table->verticalHeader()->setVisible(false);
    return table;
}

void DialogExtender::createUI()
{
    this->setWindowTitle("AxScript manager");
    this->resize(1200, 700);
    this->setProperty("Main", "base");

    /// Local Scripts
    tableWidget = createScriptTable(this, 4);
    tableWidget->setHorizontalHeaderItem(0, new QTableWidgetItem("Name"));
    tableWidget->setHorizontalHeaderItem(1, new QTableWidgetItem("Path"));
    tableWidget->setHorizontalHeaderItem(2, new QTableWidgetItem("Status"));
    tableWidget->setHorizontalHeaderItem(3, new QTableWidgetItem("Description"));
    tableWidget->hideColumn(3);

    textComment = new QTextEdit(this);
    textComment->setReadOnly(true);

    splitter = new QSplitter(Qt::Vertical, this);
    splitter->setContentsMargins(0, 0, 0, 0);
    splitter->setHandleWidth(3);
    splitter->addWidget(tableWidget);
    splitter->addWidget(textComment);
    splitter->setSizes(QList<int>({500, 140}));

    /// Server Scripts
    serverProjectCombo = new QComboBox(this);
    serverProjectCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    serverTableWidget = createScriptTable(this, 3);
    serverTableWidget->setHorizontalHeaderItem(0, new QTableWidgetItem("Name"));
    serverTableWidget->setHorizontalHeaderItem(1, new QTableWidgetItem("Status"));
    serverTableWidget->setHorizontalHeaderItem(2, new QTableWidgetItem("Description"));
    serverTableWidget->hideColumn(2);

    serverTextComment = new QTextEdit(this);
    serverTextComment->setReadOnly(true);

    serverSplitter = new QSplitter(Qt::Vertical, this);
    serverSplitter->setContentsMargins(0, 0, 0, 0);
    serverSplitter->setHandleWidth(3);
    serverSplitter->addWidget(serverTableWidget);
    serverSplitter->addWidget(serverTextComment);
    serverSplitter->setSizes(QList<int>({500, 140}));

    auto* serverLayout = new QVBoxLayout();
    serverLayout->setContentsMargins(4, 4, 4, 4);
    serverLayout->setSpacing(4);
    serverLayout->addWidget(serverProjectCombo);
    serverLayout->addWidget(serverSplitter, 1);

    serverTab = new QWidget(this);
    serverTab->setLayout(serverLayout);

    tabWidget = new QTabWidget(this);
    tabWidget->addTab(splitter, "Local Scripts");
    tabWidget->addTab(serverTab, "Server Scripts");

    spacer1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    spacer2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    buttonClose = new QPushButton("Close", this);
    buttonClose->setFixedWidth(180);

    layout = new QGridLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(tabWidget,   0, 0, 1, 3);
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
    QString baseDir;
    if (GlobalClient && GlobalClient->mainUI) {
        if (auto profile = GlobalClient->mainUI->GetCurrentProfile())
            baseDir = profile->GetProjectDir();
    }

    NonBlockingDialogs::getOpenFileName(const_cast<DialogExtender*>(this), "Load Script", baseDir, "AxScript Files (*.axs)",
        [this](const QString& filePath) {
            if (filePath.isEmpty())
                return;

            extender->LoadFromFile(filePath, true);
    });
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

/// Server Scripts

void DialogExtender::SetMainUI(MainUI* ui)
{
    mainUI = ui;
    RefreshProjectsList();
}

void DialogExtender::RefreshProjectsList()
{
    serverProjectCombo->blockSignals(true);
    serverProjectCombo->clear();

    if (!mainUI) {
        serverProjectCombo->blockSignals(false);
        return;
    }

    auto projects = mainUI->GetAdaptixProjects();
    for (auto* widget : projects) {
        if (widget && widget->GetProfile()) {
            QString projectName = widget->GetProfile()->GetProject();
            serverProjectCombo->addItem(projectName, QVariant::fromValue(static_cast<void*>(widget)));
        }
    }

    serverProjectCombo->blockSignals(false);

    if (serverProjectCombo->count() > 0) {
        serverProjectCombo->setCurrentIndex(0);
        onProjectChanged(0);
    } else {
        currentAdaptixWidget = nullptr;
        RefreshServerScripts();
    }
}

void DialogExtender::onProjectChanged(int index)
{
    if (index < 0 || !serverProjectCombo) {
        currentAdaptixWidget = nullptr;
    } else {
        currentAdaptixWidget = static_cast<AdaptixWidget*>(serverProjectCombo->itemData(index).value<void*>());
    }
    RefreshServerScripts();
}

void DialogExtender::RefreshServerScripts()
{
    serverTableWidget->setRowCount(0);
    serverTextComment->clear();

    if (!currentAdaptixWidget)
        return;

    QList<ServerScriptInfo> scripts = currentAdaptixWidget->GetServerScripts();
    for (const ServerScriptInfo &entry : scripts) {
        auto item_Name   = new QTableWidgetItem(entry.name);
        auto item_Status = new QTableWidgetItem("");
        auto item_Desc   = new QTableWidgetItem(entry.description);

        item_Name->setFlags(item_Name->flags() ^ Qt::ItemIsEditable);

        item_Status->setFlags(item_Status->flags() ^ Qt::ItemIsEditable);
        item_Status->setTextAlignment(Qt::AlignCenter);
        if (entry.enabled) {
            item_Status->setText("Enable");
            item_Status->setForeground(QColor(COLOR_NeonGreen));
        } else {
            item_Status->setText("Disable");
            item_Status->setForeground(QColor(COLOR_BrightOrange));
        }

        int row = serverTableWidget->rowCount();
        serverTableWidget->setRowCount(row + 1);

        bool isSortingEnabled = serverTableWidget->isSortingEnabled();
        serverTableWidget->setSortingEnabled(false);
        serverTableWidget->setItem(row, 0, item_Name);
        serverTableWidget->setItem(row, 1, item_Status);
        serverTableWidget->setItem(row, 2, item_Desc);
        serverTableWidget->setSortingEnabled(isSortingEnabled);
    }

    serverTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    serverTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
}

void DialogExtender::handleServerMenu(const QPoint &pos)
{
    QMenu menu;
    menu.addAction("Enable",  this, &DialogExtender::onServerActionEnable);
    menu.addAction("Disable", this, &DialogExtender::onServerActionDisable);

    QPoint globalPos = serverTableWidget->mapToGlobal(pos);
    menu.exec(globalPos);
}

void DialogExtender::onServerActionEnable()
{
    if (!currentAdaptixWidget)
        return;

    for (int row = 0; row < serverTableWidget->rowCount(); ++row) {
        if (serverTableWidget->item(row, 0)->isSelected()) {
            QString name = serverTableWidget->item(row, 0)->text();
            currentAdaptixWidget->EnableServerScript(name);
        }
    }
    RefreshServerScripts();
}

void DialogExtender::onServerActionDisable()
{
    if (!currentAdaptixWidget)
        return;

    for (int row = 0; row < serverTableWidget->rowCount(); ++row) {
        if (serverTableWidget->item(row, 0)->isSelected()) {
            QString name = serverTableWidget->item(row, 0)->text();
            currentAdaptixWidget->DisableServerScript(name);
        }
    }
    RefreshServerScripts();
}

void DialogExtender::onServerRowSelect(const int row, int column) const
{
    QTableWidgetItem* item = serverTableWidget->item(row, 2);
    if (item)
        serverTextComment->setText(item->text());
}
