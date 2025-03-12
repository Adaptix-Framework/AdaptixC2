#include <UI/Widgets/TasksWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Utils/CustomElements.h>

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

    connect(tableWidget, &QTableWidget::customContextMenuRequested, this, &TasksWidget::handleTasksMenu);
    connect(tableWidget, &QTableWidget::itemSelectionChanged,       this, &TasksWidget::onTableItemSelection);
    connect(tableWidget, &QTableWidget::itemSelectionChanged,       this, [this](){tableWidget->setFocus();} );
    connect(comboAgent,  &QComboBox::currentTextChanged,            this, &TasksWidget::onAgentChange);
    connect(comboStatus, &QComboBox::currentTextChanged,            this, &TasksWidget::onAgentChange);
    connect(inputFilter, &QLineEdit::textChanged,                   this, &TasksWidget::onAgentChange);
}

TasksWidget::~TasksWidget() = default;

void TasksWidget::createUI()
{
    auto horizontalSpacer1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto horizontalSpacer2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    comboAgent = new QComboBox(this);
    comboAgent->addItem( "All agents" );
    comboAgent->setCurrentIndex(0);

    comboStatus = new QComboBox(this);
    comboStatus->addItems( QStringList() << "Any status" << "Hosted" << "Running" << "Success" << "Error" << "Canceled" );
    comboStatus->setCurrentIndex(0);

    inputFilter = new QLineEdit(this);
    inputFilter->setPlaceholderText("filter");

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
    tableWidget->setColumnCount(10);
    tableWidget->setHorizontalHeaderItem( 0, new QTableWidgetItem("Task ID"));
    tableWidget->setHorizontalHeaderItem( 1, new QTableWidgetItem("Task Type"));
    tableWidget->setHorizontalHeaderItem( 2, new QTableWidgetItem("Agent ID"));
    tableWidget->setHorizontalHeaderItem( 3, new QTableWidgetItem("Client"));
    tableWidget->setHorizontalHeaderItem( 4, new QTableWidgetItem("Computer"));
    tableWidget->setHorizontalHeaderItem( 5, new QTableWidgetItem("Start Time"));
    tableWidget->setHorizontalHeaderItem( 6, new QTableWidgetItem("Finish Time"));
    tableWidget->setHorizontalHeaderItem( 7, new QTableWidgetItem("Commandline"));
    tableWidget->setHorizontalHeaderItem( 8, new QTableWidgetItem("Result"));
    tableWidget->setHorizontalHeaderItem( 9, new QTableWidgetItem("Output"));

    mainGridLayout = new QGridLayout(this);
    mainGridLayout->setContentsMargins( 0, 0,  0, 0);
    mainGridLayout->setVerticalSpacing(4);
    mainGridLayout->setHorizontalSpacing(8);

    mainGridLayout->addItem( horizontalSpacer1, 0, 0,  1, 1  );
    mainGridLayout->addWidget( comboAgent, 0, 1, 1, 1  );
    mainGridLayout->addWidget( comboStatus, 0, 2, 1, 1  );
    mainGridLayout->addWidget( inputFilter, 0, 3, 1, 1  );
    mainGridLayout->addItem( horizontalSpacer2, 0, 4,  1, 1  );
    mainGridLayout->addWidget( tableWidget,  1, 0,  1, 5 );

    this->setLayout(mainGridLayout);
}

bool TasksWidget::filterItem(const TaskData &task) const
{
    if( comboAgent->currentIndex() > 0 ) {
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

void TasksWidget::ClearTableContent() const {
    for (int row = tableWidget->rowCount() - 1; row >= 0; row--) {
        for (int col = 0; col < tableWidget->columnCount(); ++col)
            tableWidget->takeItem(row, col);

        tableWidget->removeRow(row);
    }
}

void TasksWidget::addTableItem(const Task* newTask) const
{
    if( tableWidget->rowCount() < 1 )
        tableWidget->setRowCount( 1 );
    else
        tableWidget->setRowCount( tableWidget->rowCount() + 1 );

    bool isSortingEnabled = tableWidget->isSortingEnabled();
    tableWidget->setSortingEnabled( false );
    tableWidget->setItem( tableWidget->rowCount() - 1, 0, newTask->item_TaskId );
    tableWidget->setItem( tableWidget->rowCount() - 1, 1, newTask->item_TaskType );
    tableWidget->setItem( tableWidget->rowCount() - 1, 2, newTask->item_AgentId );
    tableWidget->setItem( tableWidget->rowCount() - 1, 3, newTask->item_Client );
    tableWidget->setItem( tableWidget->rowCount() - 1, 4, newTask->item_Computer );
    tableWidget->setItem( tableWidget->rowCount() - 1, 5, newTask->item_StartTime );
    tableWidget->setItem( tableWidget->rowCount() - 1, 6, newTask->item_FinishTime );
    tableWidget->setItem( tableWidget->rowCount() - 1, 7, newTask->item_CommandLine );
    tableWidget->setItem( tableWidget->rowCount() - 1, 8, newTask->item_Result );
    tableWidget->setItem( tableWidget->rowCount() - 1, 9, newTask->item_Message );
    tableWidget->setSortingEnabled( isSortingEnabled );

    tableWidget->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 3, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 4, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 5, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 6, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 8, QHeaderView::ResizeToContents );

    tableWidget->setItemDelegate(new PaddingDelegate(tableWidget));
    tableWidget->verticalHeader()->setSectionResizeMode(tableWidget->rowCount() - 1, QHeaderView::ResizeToContents);
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

void TasksWidget::EditTaskSend(TaskData newTask) const
{

}


void TasksWidget::RemoveTaskItem(const QString &taskId) const
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    Task* task = adaptixWidget->TasksMap[taskId];
    QString agentId = task->data.AgentId;

    adaptixWidget->TasksMap.remove(taskId);
    adaptixWidget->TasksVector.removeOne(agentId);
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

void TasksWidget::SetData() const
{
    taskOutputConsole->SetConten("", "");

    this->ClearTableContent();

    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );

    for (int i = 0; i < adaptixWidget->TasksVector.size(); i++ ) {
        QString taskId = adaptixWidget->TasksVector[i];
        Task* task = adaptixWidget->TasksMap[taskId];
        if ( this->filterItem(task->data) )
            this->addTableItem(task);
    }
}

void TasksWidget::SetAgentFilter(const QString &agentId) const
{
    comboAgent->setCurrentText(agentId);
}

/// SLOTS

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

void TasksWidget::onTableItemSelection() const
{
    int count = tableWidget->selectedItems().size() / tableWidget->columnCount();
    if ( count != 1 )
        return;

    int row = tableWidget->selectedItems().first()->row();
    QString taskId = tableWidget->item(row,0)->text();

    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    if( !adaptixWidget->TasksMap.contains(taskId) )
        return;

    TaskData taskData = adaptixWidget->TasksMap[taskId]->data;
    taskOutputConsole->SetConten(taskData.Message, taskData.Output);
    adaptixWidget->LoadTasksOutput();
}

void TasksWidget::onAgentChange(QString agentId) const
{
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
        adaptixWidget->Agents[agentId]->TasksStop(agentTasks[agentId]);
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
        adaptixWidget->Agents[agentId]->TasksDelete(agentTasks[agentId]);
}
