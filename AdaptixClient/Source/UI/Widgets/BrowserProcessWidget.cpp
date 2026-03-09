#include <Agent/Agent.h>
#include <oclero/qlementine/widgets/Menu.hpp>
#include <Utils/CustomElements.h>
#include <UI/Widgets/BrowserProcessWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxScriptManager.h>

REGISTER_DOCK_WIDGET(BrowserProcessWidget, "Browser Process", false)

BrowserProcessWidget::BrowserProcessWidget(const AdaptixWidget* w, Agent* a) : DockTab(QString("Processes [%1]").arg(a->data.Id), w->GetProfile()->GetProject())
{
    agent = a;
    this->createUI();

    connect(buttonReload,      &QPushButton::clicked,   this, &BrowserProcessWidget::onReload);
    connect(inputFilter,       &QLineEdit::textChanged, this, &BrowserProcessWidget::onFilter);
    connect(tableView,         &QTableView::customContextMenuRequested, this, &BrowserProcessWidget::handleTableMenu );
    connect(tableView,         &QTableView::clicked, this, &BrowserProcessWidget::onTableSelect );
    connect(treeBrowserWidget, &QTreeWidget::clicked,  this, &BrowserProcessWidget::onTreeSelect );
    connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &selected, const QItemSelection &deselected){
        Q_UNUSED(selected)
        Q_UNUSED(deselected)
        if (!inputFilter->hasFocus())
            tableView->setFocus();
    });

    this->dockWidget->setWidget(this);
}

BrowserProcessWidget::~BrowserProcessWidget() = default;

void BrowserProcessWidget::createUI()
{
    buttonReload = new QPushButton(QIcon(":/icons/reload"), "", this);
    buttonReload->setIconSize( QSize( 24,24 ));
    buttonReload->setFixedSize(37, 28);
    buttonReload->setToolTip("Reload");

    inputFilter = new QLineEdit(this);

    line_1 = new QFrame(this);
    line_1->setFrameShape(QFrame::VLine);
    line_1->setMinimumHeight(25);

    statusLabel = new QLabel(this);
    statusLabel->setText("Status: ");

    loadingSpinner = new oclero::qlementine::LoadingSpinner(this);
    loadingSpinner->setFixedSize(16, 16);
    loadingSpinner->setVisible(false);

    tableModel = new QStandardItemModel(this);

    tableView = new QTableView(this );
    tableView->setModel(tableModel);
    tableView->setHorizontalHeader(new BoldHeaderView(Qt::Horizontal, tableView));
    tableView->setContextMenuPolicy( Qt::CustomContextMenu );
    tableView->setAutoFillBackground( false );
    tableView->setShowGrid( false );
    tableView->setSortingEnabled( true );
    tableView->setWordWrap( true );
    tableView->setCornerButtonEnabled( true );
    tableView->setSelectionBehavior( QAbstractItemView::SelectRows );
    tableView->setFocusPolicy( Qt::NoFocus );
    tableView->setAlternatingRowColors( true );
    tableView->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    tableView->horizontalHeader()->setCascadingSectionResizes( true );
    tableView->horizontalHeader()->setHighlightSections( false );
    tableView->verticalHeader()->setVisible( false );
    tableView->setItemDelegate(new PaddingDelegate(tableView));

    if (this->agent->data.Os == OS_WINDOWS) {
        tableModel->setColumnCount(6);
        tableModel->setHorizontalHeaderItem(0, new QStandardItem("PID"));
        tableModel->setHorizontalHeaderItem(1, new QStandardItem("PPID"));
        tableModel->setHorizontalHeaderItem(2, new QStandardItem("Arch"));
        tableModel->setHorizontalHeaderItem(3, new QStandardItem("Session"));
        tableModel->setHorizontalHeaderItem(4, new QStandardItem("Context"));
        tableModel->setHorizontalHeaderItem(5, new QStandardItem("Process"));
    }
    else {
        tableModel->setColumnCount(5);
        tableModel->setHorizontalHeaderItem(0, new QStandardItem("PID"));
        tableModel->setHorizontalHeaderItem(1, new QStandardItem("PPID"));
        tableModel->setHorizontalHeaderItem(2, new QStandardItem("TTY"));
        tableModel->setHorizontalHeaderItem(3, new QStandardItem("Context"));
        tableModel->setHorizontalHeaderItem(4, new QStandardItem("Process"));
    }

    listGridLayout = new QGridLayout(this);
    listGridLayout->setContentsMargins(5, 4, 1, 1);
    listGridLayout->setVerticalSpacing(4);
    listGridLayout->setHorizontalSpacing(8);

    listGridLayout->addWidget( buttonReload,   0, 0, 1, 1 );
    listGridLayout->addWidget( inputFilter,     0, 1, 1, 5 );
    listGridLayout->addWidget( line_1,          0, 6, 1, 1 );
    listGridLayout->addWidget( loadingSpinner,  0, 7, 1, 1 );
    listGridLayout->addWidget( statusLabel,     0, 8, 1, 1 );
    listGridLayout->addWidget( tableView,  1, 0, 1, 9 );

    listBrowserWidget = new QWidget(this);
    listBrowserWidget->setLayout(listGridLayout);

    treeBrowserWidget = new QTreeWidget();
    treeBrowserWidget->setSortingEnabled(false);
    treeBrowserWidget->headerItem()->setText( 0, "Process Tree" );
    treeBrowserWidget->setIconSize(QSize(25, 25));

    splitter = new QSplitter( this );
    splitter->setOrientation( Qt::Horizontal );
    splitter->addWidget( treeBrowserWidget );
    splitter->addWidget( listBrowserWidget );
    splitter->setSizes( QList<int>() << 110 << 200 );

    mainGridLayout = new QGridLayout(this );
    mainGridLayout->setContentsMargins(0, 0, 0, 0 );
    mainGridLayout->addWidget(splitter, 0, 0, 1, 1 );

    this->setLayout(mainGridLayout);
}

void BrowserProcessWidget::SetStatus(qint64 time, int msgType, const QString &message) const
{
    loadingSpinner->setSpinning(false);
    loadingSpinner->setVisible(false);

    QString sTime  = UnixTimestampGlobalToStringLocal(time);
    QString status;
    if( msgType == CONSOLE_OUT_LOCAL_ERROR || msgType == CONSOLE_OUT_ERROR ) {
        status = TextColorHtml(message, COLOR_ChiliPepper) + " >> " + sTime;
        statusLabel->setText(status);
    }
    else {
        status = TextColorHtml(message, COLOR_NeonGreen) + " >> " + sTime;
        statusLabel->setText(status);
    }
}

void BrowserProcessWidget::SetProcess(int msgType, const QString &data) const
{
    loadingSpinner->setSpinning(false);
    loadingSpinner->setVisible(false);

    if( msgType == CONSOLE_OUT_LOCAL_ERROR || msgType == CONSOLE_OUT_ERROR )
        return;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data.toUtf8());
    if ( !jsonDoc.isArray() )
        return;

    QJsonArray jsonArray = jsonDoc.array();

    if (agent->data.Os == OS_WINDOWS) {
        QMap<int, BrowserProcessDataWin> processMap;
        for ( const QJsonValue& value : jsonArray ) {
            QJsonObject jsonObject = value.toObject();

            BrowserProcessDataWin processData = {0};
            processData.pid     = jsonObject["b_pid"].toDouble();
            processData.ppid    = jsonObject["b_ppid"].toDouble();
            processData.sessId  = jsonObject["b_session_id"].toDouble();
            processData.arch    = jsonObject["b_arch"].toString();
            processData.context = jsonObject["b_context"].toString();
            processData.process = jsonObject["b_process_name"].toString();

            processMap[processData.pid] = processData;
        }
        this->setTableProcessDataWin(processMap);
        this->setTreeProcessDataWin(processMap);
    }
    else {
        QMap<int, BrowserProcessDataUnix> processMap;
        for ( const QJsonValue& value : jsonArray ) {
            QJsonObject jsonObject = value.toObject();

            BrowserProcessDataUnix processData = {0};
            processData.pid     = jsonObject["b_pid"].toDouble();
            processData.ppid    = jsonObject["b_ppid"].toDouble();
            processData.tty     = jsonObject["b_tty"].toString();
            processData.context = jsonObject["b_context"].toString();
            processData.process = jsonObject["b_process_name"].toString();

            processMap[processData.pid] = processData;
        }
        this->setTableProcessDataUnix(processMap);
        this->setTreeProcessDataUnix(processMap);
    }

    this->onFilter(inputFilter->text());
}

/// PRIVATE

void BrowserProcessWidget::setTableProcessDataWin(const QMap<int, BrowserProcessDataWin>& processMap) const
{
    tableModel->removeRows(0, tableModel->rowCount());

    tableModel->setRowCount(processMap.size());
    tableView->setSortingEnabled( false );

    int row = 0;
    for (const auto& item : processMap) {
        auto item_Pid = new QStandardItem( QString::number(item.pid) );
        item_Pid->setTextAlignment( Qt::AlignCenter );
        item_Pid->setFlags(item_Pid->flags() & ~Qt::ItemIsEditable);

        auto item_Ppid = new QStandardItem( QString::number(item.ppid) );
        item_Ppid->setTextAlignment( Qt::AlignCenter );
        item_Ppid->setFlags(item_Ppid->flags() & ~Qt::ItemIsEditable);

        auto item_Arch = new QStandardItem( item.arch );
        item_Arch->setTextAlignment( Qt::AlignCenter );
        item_Arch->setFlags(item_Arch->flags() & ~Qt::ItemIsEditable);

        auto item_Session = new QStandardItem( QString::number(item.sessId) );
        item_Session->setTextAlignment( Qt::AlignCenter );
        item_Session->setFlags(item_Session->flags() & ~Qt::ItemIsEditable);

        auto item_Context = new QStandardItem( item.context );
        item_Context->setFlags(item_Context->flags() & ~Qt::ItemIsEditable);

        auto item_Process = new QStandardItem( item.process );
        item_Process->setFlags(item_Process->flags() & ~Qt::ItemIsEditable);

        if ( agent->data.Pid == QString::number(item.pid) ) {
            item_Pid->setForeground(QColor(COLOR_ChiliPepper));
            item_Ppid->setForeground(QColor(COLOR_ChiliPepper));
            item_Arch->setForeground(QColor(COLOR_ChiliPepper));
            item_Session->setForeground(QColor(COLOR_ChiliPepper));
            item_Context->setForeground(QColor(COLOR_ChiliPepper));
            item_Process->setForeground(QColor(COLOR_ChiliPepper));
        }

        tableModel->setItem(row, 0, item_Pid);
        tableModel->setItem(row, 1, item_Ppid);
        tableModel->setItem(row, 2, item_Arch);
        tableModel->setItem(row, 3, item_Session);
        tableModel->setItem(row, 4, item_Context);
        tableModel->setItem(row, 5, item_Process);

        row++;
    }

    tableView->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
    tableView->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
    tableView->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
    tableView->horizontalHeader()->setSectionResizeMode( 3, QHeaderView::ResizeToContents );
    tableView->horizontalHeader()->setSectionResizeMode( 4, QHeaderView::ResizeToContents );

    tableView->setSortingEnabled( true );
}

void BrowserProcessWidget::setTableProcessDataUnix(const QMap<int, BrowserProcessDataUnix>& processMap) const
{
    tableModel->removeRows(0, tableModel->rowCount());

    tableModel->setRowCount(processMap.size());
    tableView->setSortingEnabled( false );

    int row = 0;
    for (const auto& item : processMap) {
        auto item_Pid = new QStandardItem( QString::number(item.pid) );
        item_Pid->setTextAlignment( Qt::AlignCenter );
        item_Pid->setFlags(item_Pid->flags() & ~Qt::ItemIsEditable);

        auto item_Ppid = new QStandardItem( QString::number(item.ppid) );
        item_Ppid->setTextAlignment( Qt::AlignCenter );
        item_Ppid->setFlags(item_Ppid->flags() & ~Qt::ItemIsEditable);

        auto item_Tty = new QStandardItem( item.tty );
        item_Tty->setTextAlignment( Qt::AlignCenter );
        item_Tty->setFlags(item_Tty->flags() & ~Qt::ItemIsEditable);

        auto item_Context = new QStandardItem( item.context );
        item_Context->setFlags(item_Context->flags() & ~Qt::ItemIsEditable);

        auto item_Process = new QStandardItem( item.process );
        item_Process->setFlags(item_Process->flags() & ~Qt::ItemIsEditable);

        if ( agent->data.Pid == QString::number(item.pid) ) {
            item_Pid->setForeground(QColor(COLOR_ChiliPepper));
            item_Ppid->setForeground(QColor(COLOR_ChiliPepper));
            item_Tty->setForeground(QColor(COLOR_ChiliPepper));
            item_Context->setForeground(QColor(COLOR_ChiliPepper));
            item_Process->setForeground(QColor(COLOR_ChiliPepper));
        }

        tableModel->setItem(row, 0, item_Pid);
        tableModel->setItem(row, 1, item_Ppid);
        tableModel->setItem(row, 2, item_Tty);
        tableModel->setItem(row, 3, item_Context);
        tableModel->setItem(row, 4, item_Process);

        row++;
    }

    tableView->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
    tableView->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
    tableView->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
    tableView->horizontalHeader()->setSectionResizeMode( 3, QHeaderView::ResizeToContents );

    tableView->setSortingEnabled( true );
}

void BrowserProcessWidget::setTreeProcessDataWin(QMap<int, BrowserProcessDataWin> processMap) const
{
    treeBrowserWidget->clear();

    treeBrowserWidget->setColumnCount(3);
    treeBrowserWidget->setHeaderLabels({"Process", "Process ID", "Context"});

    QMap<int, QTreeWidgetItem*> nodeMap;

    auto pids = processMap.keys();
    for (int pid : pids) {
        BrowserProcessDataWin processData = processMap[pid];
        if (processData.ppid == 0 || !pids.contains(processData.ppid)) {
            QTreeWidgetItem* rootItem = new QTreeWidgetItem(treeBrowserWidget);
            rootItem->setText(0, processData.process);
            rootItem->setText(1, QString::number(processData.pid));
            rootItem->setText(2, processData.context);

            processMap.remove(processData.pid);

            nodeMap[processData.pid] = rootItem;
            addProcessToTreeWin(rootItem, processData.pid, processMap, &nodeMap);
        }
    }

    treeBrowserWidget->header()->setStretchLastSection(false);
    treeBrowserWidget->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
    treeBrowserWidget->header()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
    treeBrowserWidget->header()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );

    treeBrowserWidget->expandAll();
}

void BrowserProcessWidget::addProcessToTreeWin(QTreeWidgetItem* parent, const int parentPID, QMap<int, BrowserProcessDataWin> processMap, QMap<int, QTreeWidgetItem*> *nodeMap)
{
    auto pids = processMap.keys();
    for (int pid : pids) {
        BrowserProcessDataWin processData = processMap[pid];
        if (processData.ppid == parentPID) {
            QTreeWidgetItem* childItem = new QTreeWidgetItem(parent);
            childItem->setText(0, processData.process);
            childItem->setText(1, QString::number(processData.pid));
            childItem->setText(2, processData.context);

            processMap.remove(processData.pid);

            (*nodeMap)[processData.pid] = childItem;
            addProcessToTreeWin(childItem, processData.pid, processMap, nodeMap);
        }
    }
}

void BrowserProcessWidget::setTreeProcessDataUnix(QMap<int, BrowserProcessDataUnix> processMap) const
{
    treeBrowserWidget->clear();

    treeBrowserWidget->setColumnCount(3);
    treeBrowserWidget->setHeaderLabels({"Process", "Process ID", "Context"});

    QMap<int, QTreeWidgetItem*> nodeMap;

    auto pids = processMap.keys();
    for (int pid : pids) {
        BrowserProcessDataUnix processData = processMap[pid];
        if (processData.ppid == 0 || !pids.contains(processData.ppid)) {
            QTreeWidgetItem* rootItem = new QTreeWidgetItem(treeBrowserWidget);
            rootItem->setText(0, processData.process);
            rootItem->setText(1, QString::number(processData.pid));
            rootItem->setText(2, processData.context);

            processMap.remove(processData.pid);

            nodeMap[processData.pid] = rootItem;
            addProcessToTreeUnix(rootItem, processData.pid, processMap, &nodeMap);
        }
    }

    treeBrowserWidget->header()->setStretchLastSection(false);
    treeBrowserWidget->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
    treeBrowserWidget->header()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
    treeBrowserWidget->header()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );

    treeBrowserWidget->expandAll();
}

void BrowserProcessWidget::addProcessToTreeUnix(QTreeWidgetItem* parent, const int parentPID, QMap<int, BrowserProcessDataUnix> processMap, QMap<int, QTreeWidgetItem*> *nodeMap)
{
    auto pids = processMap.keys();
    for (int pid : pids) {
        BrowserProcessDataUnix processData = processMap[pid];
        if (processData.ppid == parentPID) {
            QTreeWidgetItem* childItem = new QTreeWidgetItem(parent);
            childItem->setText(0, processData.process);
            childItem->setText(1, QString::number(processData.pid));
            childItem->setText(2, processData.context);

            processMap.remove(processData.pid);

            (*nodeMap)[processData.pid] = childItem;
            addProcessToTreeUnix(childItem, processData.pid, processMap, nodeMap);
        }
    }
}

void BrowserProcessWidget::filterTreeWidget(const QString &filterText) const
{
    std::function<bool(QTreeWidgetItem*)> filterItem = [&](QTreeWidgetItem* item) -> bool {
        bool match = item->text(0).contains(filterText, Qt::CaseInsensitive);

        for (int i = 0; i < item->childCount(); ++i) {
            match |= filterItem(item->child(i));
        }

        item->setHidden(!match);
        return match;
    };

    for (int i = 0; i < treeBrowserWidget->topLevelItemCount(); ++i) {
        filterItem(treeBrowserWidget->topLevelItem(i));
    }
}

void BrowserProcessWidget::filterTableWidget(const QString &filterText) const
{
    for (int row = 0; row < tableModel->rowCount(); ++row) {
        bool match = false;

        for (int col = 0; col < tableModel->columnCount(); ++col) {
            if (tableModel->item(row, col) && tableModel->item(row, col)->text().contains(filterText, Qt::CaseInsensitive)) {
                match = true;
                break;
            }
        }

        tableView->setRowHidden(row, !match);
    }
}

/// SLOTS

void BrowserProcessWidget::onReload() const
{
    statusLabel->setText("");
    loadingSpinner->setVisible(true);
    loadingSpinner->setSpinning(true);
    Q_EMIT agent->adaptixWidget->eventProcessBrowserList(agent->data.Id);
}

void BrowserProcessWidget::onFilter(const QString &text) const
{
    this->filterTreeWidget(text);
    this->filterTableWidget(text);
}

void BrowserProcessWidget::handleTableMenu(const QPoint &pos)
{
    if ( !tableView->indexAt(pos).isValid() )
        return;

    QVector<DataMenuProcessBrowser> items;
    for( int rowIndex = 0 ; rowIndex < tableModel->rowCount() ; rowIndex++ ) {
        if ( tableView->selectionModel()->isSelected(tableModel->index(rowIndex, 0)) ) {
            DataMenuProcessBrowser data;
            data.agentId = this->agent->data.Id;

            if (this->agent->data.Os == OS_WINDOWS) {
                data.pid        = tableModel->item(rowIndex, 0)->text();
                data.ppid       = tableModel->item(rowIndex, 1)->text();
                data.arch       = tableModel->item(rowIndex, 2)->text();
                data.session_id = tableModel->item(rowIndex, 3)->text();
                data.context    = tableModel->item(rowIndex, 4)->text();
                data.process    = tableModel->item(rowIndex, 5)->text();
            }
            else {
                data.pid        = tableModel->item(rowIndex, 0)->text();
                data.ppid       = tableModel->item(rowIndex, 1)->text();
                data.session_id = tableModel->item(rowIndex, 2)->text();
                data.context    = tableModel->item(rowIndex, 3)->text();
                data.process    = tableModel->item(rowIndex, 4)->text();
            }
            items.append(data);
        }
    }

    oclero::qlementine::Menu ctxMenu;

    int count = agent->adaptixWidget->ScriptManager->AddMenuProcessBrowser(&ctxMenu, items);
    if (count) {
        ctxMenu.addSeparator();
    }
    ctxMenu.addAction( "Copy PID", this, &BrowserProcessWidget::actionCopyPid);

    ctxMenu.exec(tableView->horizontalHeader()->viewport()->mapToGlobal(pos));
}

void BrowserProcessWidget::actionCopyPid() const
{
    int row = tableView->currentIndex().row();
    if( row >= 0) {
        QString pid = tableModel->item( row, 0 )->text();
        QApplication::clipboard()->setText( pid );
    }
}

void BrowserProcessWidget::onTableSelect() const
{
    QString pid = tableModel->item( tableView->currentIndex().row(), 0 )->text();
    auto item = QTreeWidgetItemIterator ( treeBrowserWidget );
    while ( *item ) {
        if ( ( *item )->text( 1 ) == pid ) {
            treeBrowserWidget->setCurrentItem( *item );
            return;
        }
        ++item;
    }
}

void BrowserProcessWidget::onTreeSelect() const
{
    QString pid = treeBrowserWidget->currentItem()->text( 1 );
    for ( int i = 0; i < tableModel->rowCount(); i++ ) {
        if ( tableModel->item( i, 0 )->text().compare( pid ) == 0 )
            tableView->setCurrentIndex( tableModel->index(i, 0) );
    }
}
