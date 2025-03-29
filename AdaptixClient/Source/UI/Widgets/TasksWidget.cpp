#include <UI/Widgets/TasksWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <MainAdaptix.h>

TaskOutputWidget::TaskOutputWidget( )
{
    this->createUI();
}

TaskOutputWidget::~TaskOutputWidget() = default;

void TaskOutputWidget::createUI()
{
    inputMessage = new QLineEdit(this);
    inputMessage->setReadOnly(true);
    inputMessage->setProperty("LineEditStyle", "console");
    inputMessage->setFont( QFont( "Hack" ));

    outputTextEdit = new QTextEdit(this);
    outputTextEdit->setReadOnly(true);
    outputTextEdit->setLineWrapMode(QTextEdit::LineWrapMode::NoWrap );
    outputTextEdit->setProperty("TextEditStyle", "console" );

    mainGridLayout = new QGridLayout(this );
    mainGridLayout->setVerticalSpacing(4 );
    mainGridLayout->setContentsMargins(0, 0, 0, 4 );
    mainGridLayout->addWidget( inputMessage, 0, 0, 1, 1 );
    mainGridLayout->addWidget( outputTextEdit, 1, 0, 1, 1);

    this->setLayout(mainGridLayout);
}

void TaskOutputWidget::SetConten(const QString &message, const QString &text) const
{
    if( message.isEmpty() )
        inputMessage->clear();
    else
        inputMessage->setText(message.toHtmlEscaped());

    if ( text.isEmpty() )
        outputTextEdit->clear();
    else
        outputTextEdit->setText( TrimmedEnds(text) );
}


TasksWidget::TasksWidget( QWidget* w )
{
    this->mainWidget = w;
    this->createUI();

    taskOutputConsole = new TaskOutputWidget();

    connect(tableWidget,  &QTableWidget::customContextMenuRequested, this, &TasksWidget::handleTasksMenu);

    connect(tableWidget->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &TasksWidget::onTableItemSelection);
    // connect(tableWidget,  &QTableWidget::itemSelectionChanged,       this, &TasksWidget::onTableItemSelection);
    connect(tableWidget,  &QTableWidget::itemSelectionChanged,       this, [this](){tableWidget->setFocus();} );
    connect(comboAgent,   &QComboBox::currentTextChanged,            this, &TasksWidget::onAgentChange);
    connect(comboStatus,  &QComboBox::currentTextChanged,            this, &TasksWidget::onAgentChange);
    connect(inputFilter,  &QLineEdit::textChanged,                   this, &TasksWidget::onAgentChange);
    connect(hideButton,   &ClickableLabel::clicked,                  this, &TasksWidget::toggleSearchPanel);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), tableWidget);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &TasksWidget::toggleSearchPanel);
}

TasksWidget::~TasksWidget() = default;

void TasksWidget::createUI()
{
    auto horizontalSpacer1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto horizontalSpacer2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    searchWidget = new QWidget(this);
    searchWidget->setVisible(false);

    comboAgent = new QComboBox(searchWidget);
    comboAgent->addItem( "All agents" );
    comboAgent->setCurrentIndex(0);
    comboAgent->setMaximumWidth(200);
    comboAgent->setFixedWidth(200);

    comboStatus = new QComboBox(searchWidget);
    comboStatus->addItems( QStringList() << "Any status" << "Hosted" << "Running" << "Success" << "Error" << "Canceled" );
    comboStatus->setCurrentIndex(0);
    comboStatus->setMaximumWidth(200);
    comboStatus->setFixedWidth(200);

    inputFilter = new QLineEdit(searchWidget);
    inputFilter->setPlaceholderText("filter");
    inputFilter->setMaximumWidth(200);

    hideButton = new ClickableLabel("X");
    hideButton->setCursor( Qt::PointingHandCursor );

    searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(4);
    searchLayout->addSpacerItem(horizontalSpacer1);
    searchLayout->addWidget(comboAgent);
    searchLayout->addWidget(comboStatus);
    searchLayout->addWidget(inputFilter);
    searchLayout->addWidget(hideButton);
    searchLayout->addSpacerItem(horizontalSpacer2);

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
    tableWidget->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    tableWidget->horizontalHeader()->setCascadingSectionResizes( true );
    tableWidget->horizontalHeader()->setHighlightSections( false );
    tableWidget->verticalHeader()->setVisible( false );
    tableWidget->setColumnCount(11);
    tableWidget->setHorizontalHeaderItem( this->ColumnTaskId,      new QTableWidgetItem("Task ID"));
    tableWidget->setHorizontalHeaderItem( this->ColumnTaskType,    new QTableWidgetItem("Task Type"));
    tableWidget->setHorizontalHeaderItem( this->ColumnAgentId,     new QTableWidgetItem("Agent ID"));
    tableWidget->setHorizontalHeaderItem( this->ColumnClient,      new QTableWidgetItem("Client"));
    tableWidget->setHorizontalHeaderItem( this->ColumnUser,        new QTableWidgetItem("User"));
    tableWidget->setHorizontalHeaderItem( this->ColumnComputer,    new QTableWidgetItem("Computer"));
    tableWidget->setHorizontalHeaderItem( this->ColumnStartTime,   new QTableWidgetItem("Start Time"));
    tableWidget->setHorizontalHeaderItem( this->ColumnFinishTime,  new QTableWidgetItem("Finish Time"));
    tableWidget->setHorizontalHeaderItem( this->ColumnCommandLine, new QTableWidgetItem("Commandline"));
    tableWidget->setHorizontalHeaderItem( this->ColumnResult,      new QTableWidgetItem("Result"));
    tableWidget->setHorizontalHeaderItem( this->ColumnOutput,      new QTableWidgetItem("Output"));

    for(int i = 0; i < 11; i++) {
        if (GlobalClient->settings->data.TasksTableColumns[i] == false)
            tableWidget->hideColumn(i);
    }

    mainGridLayout = new QGridLayout(this);
    mainGridLayout->setContentsMargins( 0, 0,  0, 0);
    mainGridLayout->setVerticalSpacing(4);
    mainGridLayout->setHorizontalSpacing(8);

    mainGridLayout->addWidget( searchWidget,  0, 0,  1, 1 );
    mainGridLayout->addWidget( tableWidget,   1, 0,  1, 1 );

    this->setLayout(mainGridLayout);
}

bool TasksWidget::filterItem(const TaskData &task) const
{
    if ( !this->showPanel )
        return true;

    if( comboAgent->currentText() != "All agents" ) {
        if (comboAgent->currentText() != task.AgentId)
            return false;
    }

    if( comboStatus->currentIndex() > 0 ) {
        if( comboStatus->currentText() != task.Status )
            return false;
    }

    if( !inputFilter->text().isEmpty() ) {
        if ( !task.Message.contains(inputFilter->text(), Qt::CaseInsensitive) && !task.CommandLine.contains(inputFilter->text(), Qt::CaseInsensitive) )
            return false;
    }

    return true;
}

void TasksWidget::addTableItem(const Task* newTask) const
{
    if( tableWidget->rowCount() < 1 )
        tableWidget->setRowCount( 1 );
    else
        tableWidget->setRowCount( tableWidget->rowCount() + 1 );

    bool isSortingEnabled = tableWidget->isSortingEnabled();
    tableWidget->setSortingEnabled( false );
    tableWidget->setItem( tableWidget->rowCount() - 1, this->ColumnTaskId,      newTask->item_TaskId );
    tableWidget->setItem( tableWidget->rowCount() - 1, this->ColumnTaskType,    newTask->item_TaskType );
    tableWidget->setItem( tableWidget->rowCount() - 1, this->ColumnAgentId,     newTask->item_AgentId );
    tableWidget->setItem( tableWidget->rowCount() - 1, this->ColumnClient,      newTask->item_Client );
    tableWidget->setItem( tableWidget->rowCount() - 1, this->ColumnUser,        newTask->item_User );
    tableWidget->setItem( tableWidget->rowCount() - 1, this->ColumnComputer,    newTask->item_Computer );
    tableWidget->setItem( tableWidget->rowCount() - 1, this->ColumnStartTime,   newTask->item_StartTime );
    tableWidget->setItem( tableWidget->rowCount() - 1, this->ColumnFinishTime,  newTask->item_FinishTime );
    tableWidget->setItem( tableWidget->rowCount() - 1, this->ColumnCommandLine, newTask->item_CommandLine );
    tableWidget->setItem( tableWidget->rowCount() - 1, this->ColumnResult,      newTask->item_Result );
    tableWidget->setItem( tableWidget->rowCount() - 1, this->ColumnOutput,      newTask->item_Message );
    tableWidget->setSortingEnabled( isSortingEnabled );

    tableWidget->horizontalHeader()->setSectionResizeMode( this->ColumnTaskId, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( this->ColumnTaskType, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( this->ColumnAgentId, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( this->ColumnClient, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( this->ColumnUser, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( this->ColumnComputer, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( this->ColumnStartTime, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( this->ColumnFinishTime, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( this->ColumnResult, QHeaderView::ResizeToContents );

    tableWidget->setItemDelegate(new PaddingDelegate(tableWidget));
    tableWidget->verticalHeader()->setSectionResizeMode(tableWidget->rowCount() - 1, QHeaderView::ResizeToContents);
}


/// PUBLIC

void TasksWidget::AddTaskItem(Task* newTask) const
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    if ( adaptixWidget->TasksMap.contains(newTask->data.TaskId) )
        return;

    adaptixWidget->TasksMap[newTask->data.TaskId] = newTask;
    adaptixWidget->TasksVector.push_back(newTask->data.TaskId);

    if( comboAgent->findText(newTask->data.AgentId) == -1 )
        comboAgent->addItem(newTask->data.AgentId);

    if( !this->filterItem(newTask->data) )
        return;

    this->addTableItem(newTask);
}

void TasksWidget::RemoveTaskItem(const QString &taskId) const
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    Task* task = adaptixWidget->TasksMap[taskId];
    QString agentId = task->data.AgentId;

    adaptixWidget->TasksMap.remove(taskId);
    adaptixWidget->TasksVector.removeOne(taskId);
    delete task;

    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if (taskId == tableWidget->item( rowIndex, 0 )->text()) {
            tableWidget->removeRow(rowIndex);
            break;
        }
    }

    bool found = false;
    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( agentId == tableWidget->item( rowIndex, 2 )->text()) {
            found = true;
            break;
        }
    }

    if ( !found ) {
        int index = comboAgent->findText(agentId);
        if (index != -1)
            comboAgent->removeItem(index);
    }
}

void TasksWidget::RemoveAgentTasksItem(const QString &agentId) const
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    for (auto key : adaptixWidget->TasksMap.keys()) {
        Task* task = adaptixWidget->TasksMap[key];
        if (task->data.AgentId == agentId) {
            adaptixWidget->TasksMap.remove(key);
            adaptixWidget->TasksVector.removeOne(key);
            delete task;
        }
    }

    int index = comboAgent->findText(agentId);
    if (index != -1)
        comboAgent->removeItem(index);

    this->SetData();
}

void TasksWidget::SetAgentFilter(const QString &agentId)
{
    this->searchWidget->setVisible(true);
    this->showPanel = true;
    comboAgent->setCurrentText(agentId);

    this->SetData();
}

void TasksWidget::SetData() const
{
    taskOutputConsole->SetConten("", "");

    this->ClearTableContent();

    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );

    for (int i = 0; i < adaptixWidget->TasksVector.size(); i++ ) {
        QString taskId = adaptixWidget->TasksVector[i];
        Task* task = adaptixWidget->TasksMap[taskId];
        if ( task && this->filterItem(task->data) )
            this->addTableItem(task);
    }
}

void TasksWidget::ClearTableContent() const
{
    QSignalBlocker blocker(tableWidget->selectionModel());
    for (int row = tableWidget->rowCount() - 1; row >= 0; row--) {
        for (int col = 0; col < tableWidget->columnCount(); ++col)
            tableWidget->takeItem(row, col);

        tableWidget->removeRow(row);
    }
}

void TasksWidget::Clear() const
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    adaptixWidget->TasksVector.clear();

    for (auto taskId : adaptixWidget->TasksMap.keys()) {
        Task* task = adaptixWidget->TasksMap[taskId];
        adaptixWidget->TasksMap.remove(taskId);
        delete task;
    }

    this->ClearTableContent();

    taskOutputConsole->SetConten("", "");

    comboAgent->clear();
    comboAgent->addItem( "All agents" );
    comboAgent->setCurrentIndex(0);
    comboStatus->setCurrentIndex(0);
    inputFilter->clear();
}



/// SLOTS

void TasksWidget::toggleSearchPanel()
{
    if (this->showPanel) {
        this->showPanel = false;
        this->searchWidget->setVisible(false);
    }
    else {
        this->showPanel = true;
        this->searchWidget->setVisible(true);

    }

    this->SetData();
}

void TasksWidget::handleTasksMenu( const QPoint &pos )
{
    if ( ! tableWidget->itemAt(pos) )
        return;

    auto ctxMenu = QMenu();

    auto ctxSep1 = new QAction();
    ctxSep1->setSeparator(true);
    auto ctxSep2 = new QAction();
    ctxSep2->setSeparator(true);

    ctxMenu.addAction( "Copy TaskID", this, &TasksWidget::actionCopyTaskId);
    ctxMenu.addAction( "Copy CommandLine", this, &TasksWidget::actionCopyCmd);
    ctxMenu.addAction(ctxSep1);
    ctxMenu.addAction( "Agent Console", this, &TasksWidget::actionOpenConsole);
    ctxMenu.addAction(ctxSep2);
    ctxMenu.addAction( "Stop Task", this, &TasksWidget::actionStop);
    ctxMenu.addAction( "Delete Task", this, &TasksWidget::actionDelete);

    ctxMenu.exec(tableWidget->horizontalHeader()->viewport()->mapToGlobal(pos));
}

void TasksWidget::onTableItemSelection(const QModelIndex &current, const QModelIndex &previous) const
{
    int row = current.row();
    if (row < 0)
        return;
    
    QString taskId = tableWidget->item(row,0)->text();

    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    if( !adaptixWidget->TasksMap.contains(taskId) )
        return;

    TaskData taskData = adaptixWidget->TasksMap[taskId]->data;
    taskOutputConsole->SetConten(taskData.Message, taskData.Output);
    adaptixWidget->LoadTasksOutput();
}

void TasksWidget::onAgentChange(QString agentId) const {
    this->SetData();
}

void TasksWidget::actionCopyTaskId() const
{
    int row = tableWidget->currentRow();
    if( row >= 0) {
        QString taskId = tableWidget->item( row, 0 )->text();
        QApplication::clipboard()->setText( taskId );
    }
}

void TasksWidget::actionCopyCmd() const
{
    int row = tableWidget->currentRow();
    if( row >= 0) {
        QString cmdLine = tableWidget->item( row, 7 )->text();
        QApplication::clipboard()->setText( cmdLine );
    }
}

void TasksWidget::actionOpenConsole() const
{
    int row = tableWidget->currentRow();
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    auto agentId = tableWidget->item( row, 2 )->text();
    adaptixWidget->LoadConsoleUI(agentId);
}

void TasksWidget::actionStop() const
{
    QMap<QString, QStringList> agentTasks;

    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( tableWidget->item(rowIndex, 0)->isSelected() ) {
            auto agentId = tableWidget->item( rowIndex, 2 )->text();
            auto taskId = tableWidget->item( rowIndex, 0 )->text();
            agentTasks[agentId].append(taskId);
        }
    }

    for( QString agentId : agentTasks.keys())
        adaptixWidget->AgentsMap[agentId]->TasksStop(agentTasks[agentId]);
}

void TasksWidget::actionDelete() const
{
    QMap<QString, QStringList> agentTasks;

    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( tableWidget->item(rowIndex, 0)->isSelected() ) {
            auto agentId = tableWidget->item( rowIndex, 2 )->text();
            auto taskId = tableWidget->item( rowIndex, 0 )->text();
            agentTasks[agentId].append(taskId);
        }
    }

    for( QString agentId : agentTasks.keys())
        adaptixWidget->AgentsMap[agentId]->TasksDelete(agentTasks[agentId]);
}
