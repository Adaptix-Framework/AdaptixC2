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
    if( message.isEmpty() ) {
        inputMessage->clear();
    }
    else {
        inputMessage->setText(message.toHtmlEscaped());
    }

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

    connect( tableWidget, &QTableWidget::customContextMenuRequested, this, &TasksWidget::handleTasksMenu );
    connect( tableWidget, &QTableWidget::itemSelectionChanged,       this, &TasksWidget::onTableItemSelection );
}

TasksWidget::~TasksWidget() = default;

void TasksWidget::createUI()
{
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
    mainGridLayout->addWidget( tableWidget,  1, 0,  1, 13 );

    this->setLayout(mainGridLayout);
}

void TasksWidget::Clear()
{

}

void TasksWidget::AddTaskItem(TaskData newTask)
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    if ( adaptixWidget->Tasks.contains(newTask.TaskId) )
        return;

    QString taskType = "";
    if ( newTask.TaskType == 1 )
        taskType = "TASK";
    else if ( newTask.TaskType == 3 )
        taskType = "JOB";
    else
        return;

    QString startTime = UnixTimestampGlobalToStringLocal(newTask.StartTime);

    auto item_TaskId      = new QTableWidgetItem( newTask.TaskId );
    auto item_TaskType    = new QTableWidgetItem( taskType );
    auto item_AgentId     = new QTableWidgetItem( newTask.AgentId );
    auto item_Client      = new QTableWidgetItem( newTask.Client );
    auto item_StartTime   = new QTableWidgetItem( startTime );
    auto item_FinishTime  = new QTableWidgetItem( startTime );
    auto item_CommandLine = new QTableWidgetItem( newTask.CommandLine );
    auto item_Result      = new QTableWidgetItem( "Running" );
    auto item_Message     = new QTableWidgetItem( "" );

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

    adaptixWidget->Tasks[newTask.TaskId] = newTask;
}

void TasksWidget::EditTaskItem(TaskData newTask)
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    if ( !adaptixWidget->Tasks.contains(newTask.TaskId) )
        return;

    adaptixWidget->Tasks[newTask.TaskId].Completed  = newTask.Completed;
    if (newTask.Completed) {
        adaptixWidget->Tasks[newTask.TaskId].FinishTime  = newTask.FinishTime;
        adaptixWidget->Tasks[newTask.TaskId].MessageType = newTask.MessageType;
    }

    if (adaptixWidget->Tasks[newTask.TaskId].Message.isEmpty())
        adaptixWidget->Tasks[newTask.TaskId].Message = newTask.Message;

    adaptixWidget->Tasks[newTask.TaskId].Output += newTask.Output;

    TaskData taskData = adaptixWidget->Tasks[newTask.TaskId];

    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, 0);
        if ( item && item->text() == taskData.TaskId ) {
            if (newTask.Completed) {
                if ( taskData.MessageType == CONSOLE_OUT_ERROR || taskData.MessageType == CONSOLE_OUT_LOCAL_ERROR ){
                    tableWidget->item(row, 7)->setText("Error");
                    tableWidget->item(row, 7)->setForeground(QColor(COLOR_ChiliPepper) );
                }
                else {
                    tableWidget->item(row, 7)->setText("Success");
                    tableWidget->item(row, 7)->setForeground(QColor(COLOR_NeonGreen) );
                }
                QString finishTime = UnixTimestampGlobalToStringLocal(newTask.FinishTime);
                tableWidget->item(row, 5)->setText(finishTime);
            }
            tableWidget->item(row, 8)->setText(taskData.Message);

            break;
        }
    }
}

void TasksWidget::RemoveTaskItem(QString taskId)
{

}

/// SLOTS

void TasksWidget::handleTasksMenu( const QPoint &pos )
{

}

void TasksWidget::onTableItemSelection()
{
    int count = tableWidget->selectedItems().size() / tableWidget->columnCount();
    if ( count != 1 )
        return;

    int row = tableWidget->selectedItems().first()->row();
    QString taskId = tableWidget->item(row,0)->text();

    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    if( !adaptixWidget->Tasks.contains(taskId) )
        return;

    TaskData taskData = adaptixWidget->Tasks[taskId];
    taskOutputConsole->SetConten(taskData.Message, taskData.Output);
    adaptixWidget->LoadTasksOutput();
}