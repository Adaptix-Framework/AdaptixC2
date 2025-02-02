#include <UI/Widgets/TasksWidget.h>
#include <UI/Widgets/AdaptixWidget.h>

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

void TaskOutputWidget::SetConten(QString message, QString text)
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
    comboStatus->addItems( QStringList() << "Any status" << "Running" << "Success" << "Error" << "Canceled" );
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
    tableWidget->setColumnCount(9);
    tableWidget->setHorizontalHeaderItem( 0, new QTableWidgetItem("Task ID"));
    tableWidget->setHorizontalHeaderItem( 1, new QTableWidgetItem("Task Type"));
    tableWidget->setHorizontalHeaderItem( 2, new QTableWidgetItem("Agent ID"));
    tableWidget->setHorizontalHeaderItem( 3, new QTableWidgetItem("Client"));
    tableWidget->setHorizontalHeaderItem( 4, new QTableWidgetItem("Start Time"));
    tableWidget->setHorizontalHeaderItem( 5, new QTableWidgetItem("Finish Time"));
    tableWidget->setHorizontalHeaderItem( 6, new QTableWidgetItem("Commandline"));
    tableWidget->setHorizontalHeaderItem( 7, new QTableWidgetItem("Result"));
    tableWidget->setHorizontalHeaderItem( 8, new QTableWidgetItem("Output"));

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

bool TasksWidget::filterItem(TaskData task)
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
        if ( !task.Message.contains(inputFilter->text(), Qt::CaseInsensitive) &&
             !task.CommandLine.contains(inputFilter->text(), Qt::CaseInsensitive)
           )
            return false;
    }

    return true;
}

void TasksWidget::addTableItem(TaskData newTask)
{
    QString taskType = "";
    if ( newTask.TaskType == 1 )
        taskType = "TASK";
    else if ( newTask.TaskType == 3 )
        taskType = "JOB";
    else if ( newTask.TaskType == 4 )
        taskType = "TUNNEL";
    else
        return;

    QString startTime = UnixTimestampGlobalToStringLocal(newTask.StartTime);

    auto item_TaskId      = new QTableWidgetItem( newTask.TaskId );
    auto item_TaskType    = new QTableWidgetItem( taskType );
    auto item_AgentId     = new QTableWidgetItem( newTask.AgentId );
    auto item_Client      = new QTableWidgetItem( newTask.Client );
    auto item_StartTime   = new QTableWidgetItem( startTime );
    auto item_FinishTime  = new QTableWidgetItem( "" );
    auto item_CommandLine = new QTableWidgetItem( newTask.CommandLine );
    auto item_Result      = new QTableWidgetItem( newTask.Status );
    auto item_Message     = new QTableWidgetItem( newTask.Message );

    if (newTask.Completed) {
        if ( newTask.Status =="Error")
            item_Result->setForeground(QColor(COLOR_ChiliPepper));
        else if ( newTask.Status == "Canceled")
            item_Result->setForeground(QColor(COLOR_BabyBlue) );
        else
            item_Result->setForeground(QColor(COLOR_NeonGreen) );

        QString finishTime = UnixTimestampGlobalToStringLocal(newTask.FinishTime);
        item_FinishTime->setText(finishTime);
    }

    item_TaskId->setFlags( item_TaskId->flags() ^ Qt::ItemIsEditable );
    item_TaskId->setTextAlignment( Qt::AlignCenter );

    item_TaskType->setFlags( item_TaskType->flags() ^ Qt::ItemIsEditable );
    item_TaskType->setTextAlignment( Qt::AlignCenter );

    item_AgentId->setFlags( item_AgentId->flags() ^ Qt::ItemIsEditable );
    item_AgentId->setTextAlignment( Qt::AlignCenter );

    item_StartTime->setFlags( item_StartTime->flags() ^ Qt::ItemIsEditable );
    item_StartTime->setTextAlignment( Qt::AlignCenter );

    item_FinishTime->setFlags( item_FinishTime->flags() ^ Qt::ItemIsEditable );
    item_FinishTime->setTextAlignment( Qt::AlignCenter );

    item_Client->setFlags( item_Client->flags() ^ Qt::ItemIsEditable );
    item_Client->setTextAlignment( Qt::AlignCenter );

    item_Result->setFlags( item_Result->flags() ^ Qt::ItemIsEditable );
    item_Result->setTextAlignment( Qt::AlignCenter );

    item_CommandLine->setFlags( item_CommandLine->flags() ^ Qt::ItemIsEditable );
    item_Message->setFlags( item_Message->flags() ^ Qt::ItemIsEditable );

    if( tableWidget->rowCount() < 1 )
        tableWidget->setRowCount( 1 );
    else
        tableWidget->setRowCount( tableWidget->rowCount() + 1 );

    bool isSortingEnabled = tableWidget->isSortingEnabled();
    tableWidget->setSortingEnabled( false );
    tableWidget->setItem( tableWidget->rowCount() - 1, 0, item_TaskId );
    tableWidget->setItem( tableWidget->rowCount() - 1, 1, item_TaskType );
    tableWidget->setItem( tableWidget->rowCount() - 1, 2, item_AgentId );
    tableWidget->setItem( tableWidget->rowCount() - 1, 3, item_Client );
    tableWidget->setItem( tableWidget->rowCount() - 1, 4, item_StartTime );
    tableWidget->setItem( tableWidget->rowCount() - 1, 5, item_FinishTime );
    tableWidget->setItem( tableWidget->rowCount() - 1, 6, item_CommandLine );
    tableWidget->setItem( tableWidget->rowCount() - 1, 7, item_Result );
    tableWidget->setItem( tableWidget->rowCount() - 1, 8, item_Message );
    tableWidget->setSortingEnabled( isSortingEnabled );

    tableWidget->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 3, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 4, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 5, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 7, QHeaderView::ResizeToContents );

    tableWidget->setItemDelegate(new PaddingDelegate(tableWidget));
    tableWidget->verticalHeader()->setSectionResizeMode(tableWidget->rowCount() - 1, QHeaderView::ResizeToContents);
}



void TasksWidget::Clear()
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    adaptixWidget->TasksMap.clear();
    adaptixWidget->TasksVector.clear();
    for (int index = tableWidget->rowCount(); index > 0; index-- )
        tableWidget->removeRow(index -1 );

    taskOutputConsole->SetConten("", "");

    comboAgent->clear();
    comboAgent->addItem( "All agents" );
    comboAgent->setCurrentIndex(0);
    comboStatus->setCurrentIndex(0);
    inputFilter->clear();
}

void TasksWidget::AddTaskItem(TaskData newTask)
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    if ( adaptixWidget->TasksMap.contains(newTask.TaskId) )
        return;

    newTask.Status = "Running";
    if (newTask.Completed) {
        if ( newTask.MessageType == CONSOLE_OUT_ERROR || newTask.MessageType == CONSOLE_OUT_LOCAL_ERROR )
            newTask.Status = "Error";
        else if ( newTask.MessageType == CONSOLE_OUT_INFO || newTask.MessageType == CONSOLE_OUT_LOCAL_INFO )
            newTask.Status = "Canceled";
        else
            newTask.Status = "Success";
    }

    adaptixWidget->TasksMap[newTask.TaskId] = newTask;
    adaptixWidget->TasksVector.push_back(newTask.TaskId);

    if( comboAgent->findText(newTask.AgentId) == -1 )
        comboAgent->addItem(newTask.AgentId);

    if( !this->filterItem(newTask) )
        return;

    this->addTableItem(newTask);
}

void TasksWidget::EditTaskItem(TaskData newTask)
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    if ( !adaptixWidget->TasksMap.contains(newTask.TaskId) )
        return;

    adaptixWidget->TasksMap[newTask.TaskId].Completed  = newTask.Completed;
    if (newTask.Completed) {
        adaptixWidget->TasksMap[newTask.TaskId].FinishTime  = newTask.FinishTime;
        adaptixWidget->TasksMap[newTask.TaskId].MessageType = newTask.MessageType;

        if ( newTask.MessageType == CONSOLE_OUT_ERROR || newTask.MessageType == CONSOLE_OUT_LOCAL_ERROR )
            adaptixWidget->TasksMap[newTask.TaskId].Status = "Error";
        else if ( newTask.MessageType == CONSOLE_OUT_INFO || newTask.MessageType == CONSOLE_OUT_LOCAL_INFO )
            adaptixWidget->TasksMap[newTask.TaskId].Status = "Canceled";
        else
            adaptixWidget->TasksMap[newTask.TaskId].Status = "Success";
    }

    if (adaptixWidget->TasksMap[newTask.TaskId].Message.isEmpty())
        adaptixWidget->TasksMap[newTask.TaskId].Message = newTask.Message;

    adaptixWidget->TasksMap[newTask.TaskId].Output += newTask.Output;

    TaskData taskData = adaptixWidget->TasksMap[newTask.TaskId];

    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, 0);
        if ( item && item->text() == taskData.TaskId ) {

            if (taskData.Completed) {
                if ( taskData.Status =="Error")
                    tableWidget->item(row, 7)->setForeground(QColor(COLOR_ChiliPepper) );
                else if ( taskData.Status == "Canceled")
                    tableWidget->item(row, 7)->setForeground(QColor(COLOR_BabyBlue) );
                else
                    tableWidget->item(row, 7)->setForeground(QColor(COLOR_NeonGreen) );

                tableWidget->item(row, 7)->setText( taskData.Status );

                QString finishTime = UnixTimestampGlobalToStringLocal(taskData.FinishTime);
                tableWidget->item(row, 5)->setText(finishTime);
            }

            tableWidget->item(row, 8)->setText(taskData.Message);

            break;
        }
    }
}

void TasksWidget::RemoveTaskItem(QString taskId)
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    auto agentId = adaptixWidget->TasksMap[taskId].AgentId;

    adaptixWidget->TasksMap.remove(taskId);
    adaptixWidget->TasksVector.removeOne(taskId);

    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, 0);
        if ( item && item->text() == taskId ) {
            tableWidget->removeRow(row);
            break;
        }
    }

    bool found = false;
    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, 2);
        if ( item && item->text() == agentId ) {
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

void TasksWidget::SetData()
{
    taskOutputConsole->SetConten("", "");

    for (int index = tableWidget->rowCount(); index > 0; index-- )
        tableWidget->removeRow(index -1 );

    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );

    for (int i = 0; i < adaptixWidget->TasksVector.size(); i++ ) {
        QString taskId = adaptixWidget->TasksVector[i];
        TaskData taskData = adaptixWidget->TasksMap[taskId];
        if ( this->filterItem(taskData) )
            this->addTableItem(taskData);
    }
}

void TasksWidget::SetAgentFilter(QString agentId)
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

void TasksWidget::onTableItemSelection()
{
    int count = tableWidget->selectedItems().size() / tableWidget->columnCount();
    if ( count != 1 )
        return;

    int row = tableWidget->selectedItems().first()->row();
    QString taskId = tableWidget->item(row,0)->text();

    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    if( !adaptixWidget->TasksMap.contains(taskId) )
        return;

    TaskData taskData = adaptixWidget->TasksMap[taskId];
    taskOutputConsole->SetConten(taskData.Message, taskData.Output);
    adaptixWidget->LoadTasksOutput();
}

void TasksWidget::onAgentChange(QString agentId)
{
    this->SetData();
}

void TasksWidget::actionCopyTaskId()
{
    int row = tableWidget->currentRow();
    if( row >= 0) {
        QString taskId = tableWidget->item( row, 0 )->text();
        QApplication::clipboard()->setText( taskId );
    }
}

void TasksWidget::actionCopyCmd()
{
    int row = tableWidget->currentRow();
    if( row >= 0) {
        QString cmdLine = tableWidget->item( row, 6 )->text();
        QApplication::clipboard()->setText( cmdLine );
    }
}

void TasksWidget::actionOpenConsole()
{
    int row = tableWidget->currentRow();
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    auto agentId = tableWidget->item( row, 2 )->text();
    adaptixWidget->LoadConsoleUI(agentId);
}

void TasksWidget::actionStop()
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

void TasksWidget::actionDelete()
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