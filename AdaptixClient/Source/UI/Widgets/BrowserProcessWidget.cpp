#include <UI/Widgets/BrowserProcessWidget.h>

BrowserProcessWidget::BrowserProcessWidget(Agent* a)
{
    agent = a;
    this->createUI();

    connect(buttonReload, &QPushButton::clicked,   this, &BrowserProcessWidget::onReload);
    connect(inputFilter,  &QLineEdit::textChanged, this, &BrowserProcessWidget::onFilter);
    connect(tableWidget,  &QTableWidget::customContextMenuRequested, this, &BrowserProcessWidget::handleTableMenu );
    connect(tableWidget,       &QTableWidget::clicked, this, &BrowserProcessWidget::onTableSelect );
    connect(treeBrowserWidget, &QTreeWidget::clicked,  this, &BrowserProcessWidget::onTreeSelect );
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

    tableWidget = new QTableWidget(this );
    tableWidget->setContextMenuPolicy( Qt::CustomContextMenu );
    tableWidget->setAutoFillBackground( false );
    tableWidget->setShowGrid( false );
    tableWidget->setSortingEnabled( true );
    tableWidget->setWordWrap( true );
    tableWidget->setCornerButtonEnabled( true );
    tableWidget->setSelectionBehavior( QAbstractItemView::SelectRows );
    tableWidget->setFocusPolicy( Qt::NoFocus );
    tableWidget->setAlternatingRowColors( true );
    tableWidget->setSelectionMode( QAbstractItemView::SingleSelection );
    tableWidget->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    tableWidget->horizontalHeader()->setCascadingSectionResizes( true );
    tableWidget->horizontalHeader()->setHighlightSections( false );
    tableWidget->verticalHeader()->setVisible( false );
    tableWidget->setColumnCount(6);
    tableWidget->setHorizontalHeaderItem(0, new QTableWidgetItem("Process ID"));
    tableWidget->setHorizontalHeaderItem(1, new QTableWidgetItem("Process PPID"));
    tableWidget->setHorizontalHeaderItem(2, new QTableWidgetItem("Arch"));
    tableWidget->setHorizontalHeaderItem(3, new QTableWidgetItem("Session"));
    tableWidget->setHorizontalHeaderItem(4, new QTableWidgetItem("Context"));
    tableWidget->setHorizontalHeaderItem(5, new QTableWidgetItem("Process Name"));

    listGridLayout = new QGridLayout(this);
    listGridLayout->setContentsMargins(5, 4, 1, 1);
    listGridLayout->setVerticalSpacing(4);
    listGridLayout->setHorizontalSpacing(8);

    listGridLayout->addWidget( buttonReload, 0, 0, 1, 1  );
    listGridLayout->addWidget( inputFilter,  0, 1, 1, 5  );
    listGridLayout->addWidget( line_1,       0, 6, 1, 1  );
    listGridLayout->addWidget( statusLabel,  0, 7, 1, 1  );
    listGridLayout->addWidget( tableWidget,  1, 0, 1, 8 );

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
    if( msgType == CONSOLE_OUT_LOCAL_ERROR || msgType == CONSOLE_OUT_ERROR )
        return;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data.toUtf8());
    if ( !jsonDoc.isArray() )
        return;

    QMap<int, BrowserProcessData> processMap;

    QJsonArray jsonArray = jsonDoc.array();
    for ( QJsonValue value : jsonArray ) {
        QJsonObject jsonObject = value.toObject();

        BrowserProcessData processData = {0};
        processData.pid     = jsonObject["b_pid"].toDouble();
        processData.ppid    = jsonObject["b_ppid"].toDouble();
        processData.sessId  = jsonObject["b_session_id"].toDouble();
        processData.arch    = jsonObject["b_arch"].toString();
        processData.context = jsonObject["b_context"].toString();
        processData.process = jsonObject["b_process_name"].toString();

        processMap[processData.pid] = processData;
    }

    this->setTableProcessData(processMap);
    this->setTreeProcessData(processMap);

    this->onFilter(inputFilter->text());
}

/// PRIVATE

void BrowserProcessWidget::setTableProcessData(QMap<int, BrowserProcessData> processMap) const
{
    for (int index = tableWidget->rowCount(); index > 0; index-- )
        tableWidget->removeRow(index -1 );

    tableWidget->setRowCount(processMap.size());
    tableWidget->setSortingEnabled( false );

    int row = 0;
    for (auto item : processMap) {
        auto item_Pid = new QTableWidgetItem( QString::number(item.pid) );
        item_Pid->setTextAlignment( Qt::AlignCenter );
        item_Pid->setFlags(item_Pid->flags() ^ Qt::ItemIsEditable);

        auto item_Ppid = new QTableWidgetItem( QString::number(item.ppid) );
        item_Ppid->setTextAlignment( Qt::AlignCenter );
        item_Ppid->setFlags(item_Ppid->flags() ^ Qt::ItemIsEditable);

        auto item_Arch = new QTableWidgetItem( item.arch );
        item_Arch->setTextAlignment( Qt::AlignCenter );
        item_Arch->setFlags(item_Arch->flags() ^ Qt::ItemIsEditable);

        auto item_Session = new QTableWidgetItem( QString::number(item.sessId) );
        item_Session->setTextAlignment( Qt::AlignCenter );
        item_Session->setFlags(item_Session->flags() ^ Qt::ItemIsEditable);

        auto item_Context = new QTableWidgetItem( item.context );
        item_Context->setFlags(item_Context->flags() ^ Qt::ItemIsEditable);

        auto item_Process = new QTableWidgetItem( item.process );
        item_Process->setFlags(item_Process->flags() ^ Qt::ItemIsEditable);

        if ( agent->data.Pid == QString::number(item.pid) ) {
            item_Pid->setForeground(QColor(COLOR_ChiliPepper));
            item_Ppid->setForeground(QColor(COLOR_ChiliPepper));
            item_Arch->setForeground(QColor(COLOR_ChiliPepper));
            item_Session->setForeground(QColor(COLOR_ChiliPepper));
            item_Context->setForeground(QColor(COLOR_ChiliPepper));
            item_Process->setForeground(QColor(COLOR_ChiliPepper));
        }

        tableWidget->setItem(row, 0, item_Pid);
        tableWidget->setItem(row, 1, item_Ppid);
        tableWidget->setItem(row, 2, item_Arch);
        tableWidget->setItem(row, 3, item_Session);
        tableWidget->setItem(row, 4, item_Context);
        tableWidget->setItem(row, 5, item_Process);

        row++;
    }

    tableWidget->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 3, QHeaderView::ResizeToContents );

    tableWidget->setSortingEnabled( true );
}

void BrowserProcessWidget::setTreeProcessData(QMap<int, BrowserProcessData> processMap) const
{
    treeBrowserWidget->clear();

    treeBrowserWidget->setColumnCount(3);
    treeBrowserWidget->setHeaderLabels({"Process Name", "Process ID", "Context"});

    QMap<int, QTreeWidgetItem*> nodeMap;

    for (BrowserProcessData processData : processMap) {
        if (processData.ppid == 0 || !processMap.contains(processData.ppid)) {
            QTreeWidgetItem* rootItem = new QTreeWidgetItem(treeBrowserWidget);
            rootItem->setText(0, processData.process);
            rootItem->setText(1, QString::number(processData.pid));
            rootItem->setText(2, processData.context);

            nodeMap[processData.pid] = rootItem;
            addProcessToTree(rootItem, processData.pid, processMap, &nodeMap);
        }
    }

    treeBrowserWidget->header()->setStretchLastSection(false);
    treeBrowserWidget->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
    treeBrowserWidget->header()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
    treeBrowserWidget->header()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );

    treeBrowserWidget->expandAll();
}

void BrowserProcessWidget::addProcessToTree(QTreeWidgetItem* parent, int parentPID, QMap<int, BrowserProcessData> processMap, QMap<int, QTreeWidgetItem*> *nodeMap)
{
    for (BrowserProcessData processData : processMap) {
        if (processData.ppid == parentPID) {
            QTreeWidgetItem* childItem = new QTreeWidgetItem(parent);
            childItem->setText(0, processData.process);
            childItem->setText(1, QString::number(processData.pid));
            childItem->setText(2, processData.context);

            (*nodeMap)[processData.pid] = childItem;
            addProcessToTree(childItem, processData.pid, processMap, nodeMap);
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
    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        bool match = false;

        for (int col = 0; col < tableWidget->columnCount(); ++col) {
            if (tableWidget->item(row, col) && tableWidget->item(row, col)->text().contains(filterText, Qt::CaseInsensitive)) {
                match = true;
                break;
            }
        }

        tableWidget->setRowHidden(row, !match);
    }
}

/// SLOTS

void BrowserProcessWidget::onReload() const
{
    QString status = agent->BrowserProcess();
    statusLabel->setText(status);
}

void BrowserProcessWidget::onFilter(const QString &text) const
{
    this->filterTreeWidget(text);
    this->filterTableWidget(text);
}

void BrowserProcessWidget::actionCopyPid() const
{
    int row = tableWidget->currentRow();
    if( row >= 0) {
        QString pid = tableWidget->item( row, 0 )->text();
        QApplication::clipboard()->setText( pid );
    }
}

void BrowserProcessWidget::handleTableMenu(const QPoint &pos)
{
    if ( ! tableWidget->itemAt(pos) )
        return;

    auto ctxMenu = QMenu();

    ctxMenu.addAction( "Copy PID", this, &BrowserProcessWidget::actionCopyPid);

    ctxMenu.exec(tableWidget->horizontalHeader()->viewport()->mapToGlobal(pos));
}

void BrowserProcessWidget::onTableSelect() const
{
    QString pid = tableWidget->item( tableWidget->currentRow(), 0 )->text();
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
    for ( int i = 0; i < tableWidget->rowCount(); i++ ) {
        if ( tableWidget->item( i, 0 )->text().compare( pid ) == 0 )
            tableWidget->setCurrentItem( tableWidget->item( i, 0 ) );
    }
}
