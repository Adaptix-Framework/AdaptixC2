#include <UI/Dialogs/DialogExtender.h>
#include <oclero/qlementine/widgets/Menu.hpp>
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

    connect(tableView,         &QTableView::customContextMenuRequested, this, &DialogExtender::handleMenu);
    connect(tableView,         &QTableView::clicked, this, [this](const QModelIndex &index) { onRowSelect(index.row(), index.column()); });
    connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &selected, const QItemSelection &deselected){
        Q_UNUSED(selected)
        Q_UNUSED(deselected)
        tableView->setFocus();
    });
    connect(serverTableWidget,  &QTableView::customContextMenuRequested, this, &DialogExtender::handleServerMenu);
    connect(serverTableWidget,  &QTableView::clicked, this, [this](const QModelIndex &index) { onServerRowSelect(index.row(), index.column()); });
    connect(serverTableWidget->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &selected, const QItemSelection &deselected){
        Q_UNUSED(selected)
        Q_UNUSED(deselected)
        serverTableWidget->setFocus();
    });
    connect(serverProjectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DialogExtender::onProjectChanged);
    connect(buttonClose,        &QPushButton::clicked,                     this, &DialogExtender::close);
}

DialogExtender::~DialogExtender()  = default;

static QTableView* createScriptTable(QWidget* parent, QStandardItemModel* model, int columnCount)
{
    model->setColumnCount(columnCount);
    auto* table = new QTableView(parent);
    table->setModel(model);
    table->setHorizontalHeader(new BoldHeaderView(Qt::Horizontal, table));
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
    table->setItemDelegate(new PaddingDelegate(table));
    return table;
}

void DialogExtender::createUI()
{
    this->setWindowTitle("AxScript manager");
    this->resize(1200, 700);
    this->setProperty("Main", "base");

    /// Local Scripts
    tableModel = new QStandardItemModel(this);
    tableView = createScriptTable(this, tableModel, 4);
    tableModel->setHorizontalHeaderItem(0, new QStandardItem("Name"));
    tableModel->setHorizontalHeaderItem(1, new QStandardItem("Path"));
    tableModel->setHorizontalHeaderItem(2, new QStandardItem("Status"));
    tableModel->setHorizontalHeaderItem(3, new QStandardItem("Description"));
    tableView->hideColumn(3);

    textComment = new QTextEdit(this);
    textComment->setReadOnly(true);

    splitter = new QSplitter(Qt::Vertical, this);
    splitter->setContentsMargins(0, 0, 0, 0);
    splitter->setHandleWidth(3);
    splitter->addWidget(tableView);
    splitter->addWidget(textComment);
    splitter->setSizes(QList<int>({500, 140}));

    /// Server Scripts
    serverTab = new QWidget(this);

    serverProjectCombo = new QComboBox(serverTab);
    serverProjectCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    serverTableModel = new QStandardItemModel(serverTab);
    serverTableWidget = createScriptTable(serverTab, serverTableModel, 3);
    serverTableModel->setHorizontalHeaderItem(0, new QStandardItem("Name"));
    serverTableModel->setHorizontalHeaderItem(1, new QStandardItem("Status"));
    serverTableModel->setHorizontalHeaderItem(2, new QStandardItem("Description"));
    serverTableWidget->hideColumn(2);

    serverTextComment = new QTextEdit(serverTab);
    serverTextComment->setReadOnly(true);

    serverSplitter = new QSplitter(Qt::Vertical, serverTab);
    serverSplitter->setContentsMargins(0, 0, 0, 0);
    serverSplitter->setHandleWidth(3);
    serverSplitter->addWidget(serverTableWidget);
    serverSplitter->addWidget(serverTextComment);
    serverSplitter->setSizes(QList<int>({500, 140}));

    auto* serverLayout = new QVBoxLayout(serverTab);
    serverLayout->setContentsMargins(4, 4, 4, 4);
    serverLayout->setSpacing(4);
    serverLayout->addWidget(serverProjectCombo);
    serverLayout->addWidget(serverSplitter, 1);

    segmentedControl = new oclero::qlementine::SegmentedControl(this);
    segmentedControl->addItem("Local Scripts");
    segmentedControl->addItem("Server Scripts");
    segmentedControl->setCurrentIndex(0);

    stackWidget = new QStackedWidget(this);
    stackWidget->addWidget(splitter);
    stackWidget->addWidget(serverTab);

    connect(segmentedControl, &oclero::qlementine::SegmentedControl::currentIndexChanged, this, [this]() {
        stackWidget->setCurrentIndex(segmentedControl->currentIndex());
    });

    spacer1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    spacer2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    buttonClose = new QPushButton("Close", this);
    buttonClose->setFixedWidth(180);

    layout = new QGridLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(segmentedControl, 0, 0, 1, 3, Qt::AlignCenter);
    layout->addWidget(stackWidget,      1, 0, 1, 3);
    layout->addItem(  spacer1,          2, 0, 1, 1);
    layout->addWidget(buttonClose,      2, 1, 1, 1);
    layout->addItem(  spacer2,          2, 2, 1, 1);

    this->setLayout(layout);
}

void DialogExtender::AddExtenderItem(const ExtensionFile &extenderItem) const
{
    auto item_Name   = new QStandardItem(extenderItem.Name);
    auto item_Path   = new QStandardItem(extenderItem.FilePath);
    auto item_Desc   = new QStandardItem(extenderItem.Description);
    auto item_Status = new QStandardItem("");

    item_Name->setFlags( item_Name->flags() & ~Qt::ItemIsEditable );

    item_Path->setFlags( item_Path->flags() & ~Qt::ItemIsEditable );

    item_Status->setFlags( item_Status->flags() & ~Qt::ItemIsEditable );
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

    if( tableModel->rowCount() < 1 )
        tableModel->setRowCount( 1 );
    else
        tableModel->setRowCount( tableModel->rowCount() + 1 );

    bool isSortingEnabled = tableView->isSortingEnabled();
    tableView->setSortingEnabled( false );
    tableModel->setItem( tableModel->rowCount() - 1, 0, item_Name );
    tableModel->setItem( tableModel->rowCount() - 1, 1, item_Path );
    tableModel->setItem( tableModel->rowCount() - 1, 2, item_Status );
    tableModel->setItem( tableModel->rowCount() - 1, 3, item_Desc );
    tableView->setSortingEnabled( isSortingEnabled );

    tableView->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
    tableView->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
}

void DialogExtender::UpdateExtenderItem(const ExtensionFile &extenderItem) const
{
    for (int row = 0; row < tableModel->rowCount(); ++row) {
        QStandardItem *item = tableModel->item(row, 1);
        if ( item && item->text() == extenderItem.FilePath ) {
            tableModel->item(row, 0)->setText(extenderItem.Name);
            tableModel->item(row, 3)->setText(extenderItem.Description);

            if ( extenderItem.Enabled ) {
                tableModel->item(row, 2)->setText("Enable");
                tableModel->item(row, 2)->setForeground(QColor(COLOR_NeonGreen));
            }
            else {
                if (extenderItem.Message.isEmpty()) {
                    tableModel->item(row, 2)->setText("Disable");
                    tableModel->item(row, 2)->setForeground(QColor(COLOR_BrightOrange));
                }
                else {
                    tableModel->item(row, 2)->setText("Failed");
                    tableModel->item(row, 2)->setForeground(QColor(COLOR_ChiliPepper));
                }
            }
            break;
        }
    }
}

void DialogExtender::RemoveExtenderItem(const ExtensionFile &extenderItem) const
{
    for (int row = 0; row < tableModel->rowCount(); ++row) {
        QStandardItem *item = tableModel->item(row, 1);
        if ( item && item->text() == extenderItem.FilePath ) {
            tableModel->removeRow(row);
            break;
        }
    }
}

/// SLOTS

void DialogExtender::handleMenu(const QPoint &pos ) const
{
    oclero::qlementine::Menu menu;

    menu.addAction("Load new", this, &DialogExtender::onActionLoad );

    bool hasSelection = tableView->selectionModel()->hasSelection();
    bool allEnabled = true;
    bool allDisabled = true;

    if (hasSelection) {
        for (int rowIndex = 0; rowIndex < tableModel->rowCount(); rowIndex++) {
            if (tableView->selectionModel()->isSelected(tableModel->index(rowIndex, 0))) {
                QString status = tableModel->item(rowIndex, 2)->text();
                if (status == "Enable")
                    allDisabled = false;
                else
                    allEnabled = false;
            }
        }
    }

    auto* reloadAction = menu.addAction("Reload", this, &DialogExtender::onActionReload );
    reloadAction->setEnabled(hasSelection);
    menu.addSeparator();
    auto* enableAction = menu.addAction("Enable",  this, &DialogExtender::onActionEnable );
    enableAction->setEnabled(hasSelection && !allEnabled);
    auto* disableAction = menu.addAction("Disable", this, &DialogExtender::onActionDisable );
    disableAction->setEnabled(hasSelection && !allDisabled);
    menu.addSeparator();
    auto* removeAction = menu.addAction("Remove", this, &DialogExtender::onActionRemove );
    removeAction->setEnabled(hasSelection);

    QPoint globalPos = tableView->mapToGlobal(pos);
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
    for( int rowIndex = 0 ; rowIndex < tableModel->rowCount() ; rowIndex++ ) {
        if ( tableView->selectionModel()->isSelected(tableModel->index(rowIndex, 0)) ) {
            auto filePath = tableModel->item( rowIndex, 1 )->text();
            extender->RemoveExtension(filePath);
            extender->LoadFromFile(filePath, true);
        }
    }
}

void DialogExtender::onActionEnable() const
{
    for( int rowIndex = 0 ; rowIndex < tableModel->rowCount() ; rowIndex++ ) {
        if ( tableView->selectionModel()->isSelected(tableModel->index(rowIndex, 0)) ) {
            auto filePath = tableModel->item( rowIndex, 1 )->text();
            extender->EnableExtension(filePath);
        }
    }
}

void DialogExtender::onActionDisable() const
{
    for( int rowIndex = 0 ; rowIndex < tableModel->rowCount() ; rowIndex++ ) {
        if ( tableView->selectionModel()->isSelected(tableModel->index(rowIndex, 0)) ) {
            auto filePath = tableModel->item( rowIndex, 1 )->text();
            extender->DisableExtension(filePath);
        }
    }
}

void DialogExtender::onActionRemove() const
{
    QStringList FilesList;
    for( int rowIndex = 0 ; rowIndex < tableModel->rowCount() ; rowIndex++ ) {
        if ( tableView->selectionModel()->isSelected(tableModel->index(rowIndex, 0)) ) {
            auto filePath = tableModel->item( rowIndex, 1 )->text();
            FilesList.append(filePath);
        }
    }

    for(auto filePath : FilesList)
        extender->RemoveExtension(filePath);

    textComment->clear();
}

void DialogExtender::onRowSelect(const int row, int column) const { textComment->setText(tableModel->item(row,3)->text()); }

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
    serverTableModel->setRowCount(0);
    serverTextComment->clear();

    if (!currentAdaptixWidget)
        return;

    QList<ServerScriptInfo> scripts = currentAdaptixWidget->GetServerScripts();
    for (const ServerScriptInfo &entry : scripts) {
        auto item_Name   = new QStandardItem(entry.name);
        auto item_Status = new QStandardItem("");
        auto item_Desc   = new QStandardItem(entry.description);

        item_Name->setFlags(item_Name->flags() & ~Qt::ItemIsEditable);

        item_Status->setFlags(item_Status->flags() & ~Qt::ItemIsEditable);
        item_Status->setTextAlignment(Qt::AlignCenter);
        if (entry.enabled) {
            item_Status->setText("Enable");
            item_Status->setForeground(QColor(COLOR_NeonGreen));
        } else {
            item_Status->setText("Disable");
            item_Status->setForeground(QColor(COLOR_BrightOrange));
        }

        int row = serverTableModel->rowCount();
        serverTableModel->setRowCount(row + 1);

        bool isSortingEnabled = serverTableWidget->isSortingEnabled();
        serverTableWidget->setSortingEnabled(false);
        serverTableModel->setItem(row, 0, item_Name);
        serverTableModel->setItem(row, 1, item_Status);
        serverTableModel->setItem(row, 2, item_Desc);
        serverTableWidget->setSortingEnabled(isSortingEnabled);
    }

    serverTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    serverTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
}

void DialogExtender::handleServerMenu(const QPoint &pos)
{
    oclero::qlementine::Menu menu;

    bool hasSelection = serverTableWidget->selectionModel()->hasSelection();
    bool allEnabled = true;
    bool allDisabled = true;

    if (hasSelection) {
        for (int rowIndex = 0; rowIndex < serverTableModel->rowCount(); rowIndex++) {
            if (serverTableWidget->selectionModel()->isSelected(serverTableModel->index(rowIndex, 0))) {
                QString status = serverTableModel->item(rowIndex, 1)->text();
                if (status == "Enable")
                    allDisabled = false;
                else
                    allEnabled = false;
            }
        }
    }

    auto* enableAction = menu.addAction("Enable",  this, &DialogExtender::onServerActionEnable);
    enableAction->setEnabled(hasSelection && !allEnabled);
    auto* disableAction = menu.addAction("Disable", this, &DialogExtender::onServerActionDisable);
    disableAction->setEnabled(hasSelection && !allDisabled);

    QPoint globalPos = serverTableWidget->mapToGlobal(pos);
    menu.exec(globalPos);
}

void DialogExtender::onServerActionEnable()
{
    if (!currentAdaptixWidget)
        return;

    for (int row = 0; row < serverTableModel->rowCount(); ++row) {
        if (serverTableWidget->selectionModel()->isSelected(serverTableModel->index(row, 0))) {
            QString name = serverTableModel->item(row, 0)->text();
            currentAdaptixWidget->EnableServerScript(name);
        }
    }
    RefreshServerScripts();
}

void DialogExtender::onServerActionDisable()
{
    if (!currentAdaptixWidget)
        return;

    for (int row = 0; row < serverTableModel->rowCount(); ++row) {
        if (serverTableWidget->selectionModel()->isSelected(serverTableModel->index(row, 0))) {
            QString name = serverTableModel->item(row, 0)->text();
            currentAdaptixWidget->DisableServerScript(name);
        }
    }
    RefreshServerScripts();
}

void DialogExtender::onServerRowSelect(const int row, int column) const
{
    QStandardItem* item = serverTableModel->item(row, 2);
    if (item)
        serverTextComment->setText(item->text());
}
