#include <Agent/Agent.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/TasksWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/Settings.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Utils/FontManager.h>
#include <MainAdaptix.h>


TaskOutputWidget::TaskOutputWidget() { this->createUI(); }

TaskOutputWidget::~TaskOutputWidget() = default;

void TaskOutputWidget::createUI()
{
    inputMessage = new QLineEdit(this);
    inputMessage->setReadOnly(true);
    inputMessage->setProperty("LineEditStyle", "console");
    inputMessage->setFont( FontManager::instance().getFont("Hack") );

    outputTextEdit = new QTextEdit(this);
    outputTextEdit->setReadOnly(true);
    outputTextEdit->setWordWrapMode(QTextOption::WrapAnywhere);
    outputTextEdit->setProperty("TextEditStyle", "console" );

    mainGridLayout = new QGridLayout(this );
    mainGridLayout->setVerticalSpacing(4 );
    mainGridLayout->setContentsMargins(0, 0, 0, 4 );
    mainGridLayout->addWidget( inputMessage, 0, 0, 1, 1 );
    mainGridLayout->addWidget( outputTextEdit, 1, 0, 1, 1 );

    this->setLayout(mainGridLayout);
}

void TaskOutputWidget::SetConten(const QString &message, const QString &text) const
{
    if( message.isEmpty() )
        inputMessage->clear();
    else
        inputMessage->setText(TrimmedEnds(message).toHtmlEscaped());

    if ( text.isEmpty() )
        outputTextEdit->clear();
    else
        outputTextEdit->setText( TrimmedEnds(text) );
}





TasksWidget::TasksWidget( AdaptixWidget* w )
{
    this->adaptixWidget = w;

    this->createUI();

    taskOutputConsole = new TaskOutputWidget();

    dockWidgetTable = new KDDockWidgets::QtWidgets::DockWidget( + "Tasks:Dock-" + w->GetProfile()->GetProject(), KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    dockWidgetTable->setTitle("Tasks");
    dockWidgetTable->setWidget(this);
    dockWidgetTable->setIcon(QIcon( ":/icons/job" ), KDDockWidgets::IconPlace::TabBar);

    dockWidgetOutput = new KDDockWidgets::QtWidgets::DockWidget( + "Task Output:Dock-" + w->GetProfile()->GetProject(), KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    dockWidgetOutput->setTitle("Task Output");
    dockWidgetOutput->setWidget(taskOutputConsole);
    dockWidgetOutput->setIcon(QIcon( ":/icons/job" ), KDDockWidgets::IconPlace::TabBar);

    connect(tableView,  &QTableWidget::customContextMenuRequested, this, &TasksWidget::handleTasksMenu);
    connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &selected, const QItemSelection &deselected){
        Q_UNUSED(selected)
        Q_UNUSED(deselected)
        if (!inputFilter->hasFocus())
            tableView->setFocus();
    });

    connect(tableView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &TasksWidget::onTableItemSelection);
    connect(inputFilter,    &QLineEdit::textChanged,         this, &TasksWidget::onFilterChanged);
    connect(inputFilter,    &QLineEdit::returnPressed,       this, [this]() { proxyModel->setTextFilter(inputFilter->text()); });
    connect(comboAgent,     &QComboBox::currentTextChanged,  this, &TasksWidget::onFilterChanged);
    connect(comboType,      &QComboBox::currentTextChanged,  this, &TasksWidget::onFilterChanged);
    connect(comboStatus,    &QComboBox::currentTextChanged,  this, &TasksWidget::onFilterChanged);
    connect(hideButton,     &ClickableLabel::clicked,        this, &TasksWidget::toggleSearchPanel);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), this);
    shortcutSearch->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &TasksWidget::toggleSearchPanel);

    auto shortcutEsc = new QShortcut(QKeySequence(Qt::Key_Escape), inputFilter);
    shortcutEsc->setContext(Qt::WidgetShortcut);
    connect(shortcutEsc, &QShortcut::activated, this, [this]() { searchWidget->setVisible(false); });
}

TasksWidget::~TasksWidget()
{
    if (dockWidgetTable) {
        dockWidgetTable->setWidget(nullptr);
        delete dockWidgetTable;
        dockWidgetTable = nullptr;
    }
    if (dockWidgetOutput) {
        dockWidgetOutput->setWidget(nullptr);
        delete dockWidgetOutput;
        dockWidgetOutput = nullptr;
    }
}

KDDockWidgets::QtWidgets::DockWidget* TasksWidget::dockTasks() { return this->dockWidgetTable; }

KDDockWidgets::QtWidgets::DockWidget * TasksWidget::dockTasksOutput() { return this->dockWidgetOutput; }

void TasksWidget::SetUpdatesEnabled(const bool enabled)
{
    if (!enabled) {
        bufferingEnabled = true;
    } else {
        bufferingEnabled = false;
        flushPendingTasks();
    }

    if (proxyModel)
        proxyModel->setDynamicSortFilter(enabled);
    if (tableView)
        tableView->setSortingEnabled(enabled);

    tableView->setUpdatesEnabled(enabled);
    taskOutputConsole->setUpdatesEnabled(enabled);
}

void TasksWidget::flushPendingTasks()
{
    if (pendingTasks.isEmpty())
        return;

    tasksModel->add(pendingTasks);

    QSet<QString> agents;
    for (const auto& task : pendingTasks)
        agents.insert(task.AgentId);

    for (const QString& agentId : agents) {
        if (comboAgent->findText(agentId) == -1)
            comboAgent->addItem(agentId);
    }

    pendingTasks.clear();
}

void TasksWidget::createUI()
{
    auto horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    searchWidget = new QWidget(this);
    searchWidget->setVisible(false);

    inputFilter = new QLineEdit(searchWidget);
    inputFilter->setPlaceholderText("filter: (adm | user) & cmd");
    inputFilter->setMaximumWidth(250);

    autoSearchCheck = new QCheckBox("auto", searchWidget);
    autoSearchCheck->setChecked(true);
    autoSearchCheck->setToolTip("Auto search on text change. If unchecked, press Enter to search.");

    comboAgent = new QComboBox(searchWidget);
    comboAgent->setMinimumWidth(120);
    comboAgent->setEditable(true);
    comboAgent->setInsertPolicy(QComboBox::NoInsert);
    comboAgent->addItem("All agents");

    comboType = new QComboBox(searchWidget);
    comboType->setMinimumWidth(100);
    comboType->addItem("All types");

    comboStatus = new QComboBox(searchWidget);
    comboStatus->setMinimumWidth(100);
    comboStatus->addItems(QStringList() << "Any status" << "Hosted" << "Running" << "Success" << "Error" << "Canceled");

    hideButton = new ClickableLabel("  x  ");
    hideButton->setCursor(Qt::PointingHandCursor);

    searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(0, 4, 0, 0);
    searchLayout->setSpacing(4);
    searchLayout->addWidget(inputFilter);
    searchLayout->addWidget(autoSearchCheck);
    searchLayout->addSpacing(8);
    searchLayout->addWidget(comboAgent);
    searchLayout->addSpacing(8);
    searchLayout->addWidget(comboType);
    searchLayout->addSpacing(8);
    searchLayout->addWidget(comboStatus);
    searchLayout->addSpacing(8);
    searchLayout->addWidget(hideButton);
    searchLayout->addSpacerItem(horizontalSpacer);

    tasksModel = new TasksTableModel(this);
    proxyModel = new TasksFilterProxyModel(this);
    proxyModel->setSourceModel(tasksModel);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    tableView = new QTableView(this );
    tableView->setModel(proxyModel);
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
    tableView->horizontalHeader()->setSectionResizeMode( QHeaderView::ResizeToContents );
    tableView->horizontalHeader()->setCascadingSectionResizes( true );
    tableView->horizontalHeader()->setHighlightSections( false );
    tableView->verticalHeader()->setVisible( false );
    tableView->verticalHeader()->setSectionResizeMode( QHeaderView::ResizeToContents );

    tableView->sortByColumn(TC_StartTime, Qt::AscendingOrder);

    tableView->horizontalHeader()->setSectionResizeMode( TC_CommandLine, QHeaderView::Stretch );
    tableView->horizontalHeader()->setSectionResizeMode( TC_Output,      QHeaderView::Stretch );

    tableView->setItemDelegate(new PaddingDelegate(tableView));
    tableView->setItemDelegateForColumn(TC_CommandLine, new WrapAnywhereDelegate(tableView));
    tableView->setItemDelegateForColumn(TC_Output, new WrapAnywhereDelegate(tableView));

    this->UpdateColumnsVisible();

    mainGridLayout = new QGridLayout(this);
    mainGridLayout->setContentsMargins( 0, 0,  0, 0);
    mainGridLayout->setVerticalSpacing(4);
    mainGridLayout->setHorizontalSpacing(8);

    mainGridLayout->addWidget( searchWidget, 0, 0, 1, 1 );
    mainGridLayout->addWidget( tableView,    1, 0, 1, 1 );

    this->setLayout(mainGridLayout);
}



void TasksWidget::AddTaskItem(TaskData newTask)
{
    if ( adaptixWidget->TasksMap.contains(newTask.TaskId) )
        return;

    newTask.Status = "Hosted";
    if (newTask.Completed) {
        if ( newTask.MessageType == CONSOLE_OUT_ERROR || newTask.MessageType == CONSOLE_OUT_LOCAL_ERROR ) {
            newTask.Status = "Error";
        }
        else if ( newTask.MessageType == CONSOLE_OUT_INFO || newTask.MessageType == CONSOLE_OUT_LOCAL_INFO ) {
            newTask.Status = "Canceled";
        }
        else {
            newTask.Status = "Success";
        }
    }
    else if (newTask.TaskType == 4) {
        newTask.Status = "Running";
    }
    adaptixWidget->TasksMap[newTask.TaskId] = newTask;

    if (bufferingEnabled) {
        pendingTasks.append(newTask);
    } else {
        tasksModel->add(newTask);

        if (comboAgent->findText(newTask.AgentId) == -1)
            comboAgent->addItem(newTask.AgentId);

        if (adaptixWidget->IsSynchronized())
            this->UpdateColumnsSize();
    }
}

void TasksWidget::UpdateTaskItem(const QString &taskId, const TaskData &task) const
{
    tasksModel->update(taskId, task);
}

void TasksWidget::RemoveTaskItem(const QString &taskId) const
{
    if ( !adaptixWidget->TasksMap.contains((taskId)))
        return;

    TaskData task = adaptixWidget->TasksMap[taskId];
    QString agentId = task.AgentId;

    adaptixWidget->TasksMap.remove(taskId);

    tasksModel->remove(taskId);
}

void TasksWidget::RemoveAgentTasksItem(const QString &agentId) const
{
    for (auto key : adaptixWidget->TasksMap.keys()) {
        TaskData task = adaptixWidget->TasksMap[key];
        if (task.AgentId == agentId) {
            adaptixWidget->TasksMap.remove(key);

            tasksModel->remove(task.TaskId);
        }
    }

    int index = comboAgent->findText(agentId);
    if (index != -1)
        comboAgent->removeItem(index);

    tableView->reset();
}

void TasksWidget::SetAgentFilter(const QString &agentId)
{
    this->searchWidget->setVisible(true);
    this->showPanel = true;
    this->comboAgent->setCurrentText(agentId);

    this->onFilterChanged();
}

void TasksWidget::UpdateColumnsVisible() const
{
    for(int i = 0; i < 11; i++) {
        if (GlobalClient->settings->data.TasksTableColumns[i])
            tableView->showColumn(i);
        else
            tableView->hideColumn(i);
    }
}

void TasksWidget::UpdateColumnsSize() const
{
    tableView->resizeColumnToContents(TC_Client);
    tableView->resizeColumnToContents(TC_User);
    tableView->resizeColumnToContents(TC_Computer);
    tableView->resizeColumnToContents(TC_Result);
}

void TasksWidget::Clear() const
{
    adaptixWidget->TasksMap.clear();

    taskOutputConsole->SetConten("", "");

    tasksModel->clear();
    comboAgent->clear();
    comboAgent->addItem("All agents");
    comboType->clear();
    comboType->addItem("All types");
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
        inputFilter->setFocus();
    }
}

void TasksWidget::handleTasksMenu( const QPoint &pos )
{
    QModelIndex index = tableView->indexAt(pos);
    if (!index.isValid()) return;

    bool cancel      = false;
    bool job_running = false;
    bool remove      = false;

    QStringList taskIds;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString result = tasksModel->data(tasksModel->index(sourceIndex.row(), TC_Result), Qt::DisplayRole).toString();
        QString type   = tasksModel->data(tasksModel->index(sourceIndex.row(), TC_TaskType), Qt::DisplayRole).toString();
        QString taskId = tasksModel->data(tasksModel->index(sourceIndex.row(), TC_TaskId), Qt::DisplayRole).toString();
        taskIds.append(taskId);

        if ( result == "Hosted" )
            cancel = true;
        else if (result == "Running" && type == "JOB")
            job_running = true;
        else
            remove = true;
    }

    auto ctxMenu = QMenu();
    ctxMenu.addAction("Copy taskID", this, &TasksWidget::actionCopyTaskId);
    ctxMenu.addAction("Copy commandLine", this, &TasksWidget::actionCopyCmd);
    ctxMenu.addSeparator();
    ctxMenu.addAction("Agent console", this, &TasksWidget::actionOpenConsole);
    ctxMenu.addSeparator();

    int taskCount = adaptixWidget->ScriptManager->AddMenuTask(&ctxMenu, "Tasks", taskIds);
    int jobCount  = 0;
    if (job_running)
    jobCount = adaptixWidget->ScriptManager->AddMenuTask(&ctxMenu, "TasksJob", taskIds);
    if (taskCount + jobCount > 0)
    ctxMenu.addSeparator();

    if (cancel)
    ctxMenu.addAction("Cancel", this, &TasksWidget::actionCancel);
    if (remove)
    ctxMenu.addAction("Delete task", this, &TasksWidget::actionDelete);

    ctxMenu.exec(tableView->viewport()->mapToGlobal(pos));
}

void TasksWidget::onTableItemSelection(const QModelIndex &current, const QModelIndex &previous) const
{
    Q_UNUSED(previous);

    auto idx = tableView->currentIndex();
    if (!idx.isValid())
        return;

    QString taskId = proxyModel->index(idx.row(), TC_TaskId).data().toString();
    if (taskId.isEmpty())
        return;

    if (!adaptixWidget->TasksMap.contains(taskId))
        return;

    TaskData taskData = adaptixWidget->TasksMap[taskId];
    taskOutputConsole->SetConten(taskData.Message, taskData.Output);
    adaptixWidget->LoadTasksOutput();
}

void TasksWidget::onFilterChanged() const
{
    auto *f = qobject_cast<TasksFilterProxyModel *>(proxyModel);
    if (!f) return;

    f->setAgentFilter(comboAgent->currentText());
    f->setTypeFilter(comboType->currentText());
    f->setStatusFilter(comboStatus->currentText());
    if (autoSearchCheck->isChecked()) {
        f->setTextFilter(inputFilter->text());
    }
    UpdateColumnsSize();
}

void TasksWidget::UpdateFilterComboBoxes() const
{
    QSet<QString> agents;
    QSet<QString> types;

    for (const auto& task : adaptixWidget->TasksMap) {
        if (!task.AgentId.isEmpty())
            agents.insert(task.AgentId);
        QString typeStr = task.TaskType == 1 ? "TASK" :
                          task.TaskType == 3 ? "JOB" :
                          task.TaskType == 4 ? "TUNNEL" : "unknown";
        types.insert(typeStr);
    }

    QString currentAgent = comboAgent->currentText();
    QString currentType = comboType->currentText();

    comboAgent->blockSignals(true);
    comboType->blockSignals(true);

    comboAgent->clear();
    comboAgent->addItem("All agents");
    QStringList agentList = agents.values();
    agentList.sort();
    comboAgent->addItems(agentList);

    comboType->clear();
    comboType->addItem("All types");
    QStringList typeList = types.values();
    typeList.sort();
    comboType->addItems(typeList);

    int agentIdx = comboAgent->findText(currentAgent);
    if (agentIdx >= 0)
        comboAgent->setCurrentIndex(agentIdx);
    else
        comboAgent->setCurrentText(currentAgent);

    int typeIdx = comboType->findText(currentType);
    if (typeIdx >= 0)
        comboType->setCurrentIndex(typeIdx);
    else
        comboType->setCurrentText(currentType);

    comboAgent->blockSignals(false);
    comboType->blockSignals(false);
}

void TasksWidget::actionCopyTaskId() const
{
    auto idx = tableView->currentIndex();
    if (idx.isValid()) {
        QString taskId = proxyModel->index(idx.row(), TC_TaskId).data().toString();
        QApplication::clipboard()->setText(taskId);
    }
}

void TasksWidget::actionCopyCmd() const
{
    auto idx = tableView->currentIndex();
    if (idx.isValid()) {
        QString cmdLine = proxyModel->index(idx.row(), TC_CommandLine).data().toString();
        QApplication::clipboard()->setText(cmdLine);
    }
}

void TasksWidget::actionOpenConsole() const
{
    auto idx = tableView->currentIndex();
    if (idx.isValid()) {
        QString agentId = proxyModel->index(idx.row(), TC_AgentId).data().toString();
        adaptixWidget->LoadConsoleUI(agentId);
    }
}

void TasksWidget::actionCancel() const
{
    QMap<QString, QStringList> agentTasks;

    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString agentId = tasksModel->data(tasksModel->index(sourceIndex.row(), TC_AgentId), Qt::DisplayRole).toString();
        QString taskId  = tasksModel->data(tasksModel->index(sourceIndex.row(), TC_TaskId), Qt::DisplayRole).toString();
        if (!agentId.isEmpty() && !taskId.isEmpty())
            agentTasks[agentId].append(taskId);
    }

    for (auto it = agentTasks.begin(); it != agentTasks.end(); ++it) {
        if (adaptixWidget->AgentsMap.contains(it.key()))
            adaptixWidget->AgentsMap[it.key()]->TasksCancel(it.value());
    }
}

void TasksWidget::actionDelete() const
{
    QMap<QString, QStringList> agentTasks;

    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString agentId = tasksModel->data(tasksModel->index(sourceIndex.row(), TC_AgentId), Qt::DisplayRole).toString();
        QString taskId  = tasksModel->data(tasksModel->index(sourceIndex.row(), TC_TaskId), Qt::DisplayRole).toString();
        if (!agentId.isEmpty() && !taskId.isEmpty())
            agentTasks[agentId].append(taskId);
    }

    for (auto it = agentTasks.begin(); it != agentTasks.end(); ++it) {
        if (adaptixWidget->AgentsMap.contains(it.key()))
            adaptixWidget->AgentsMap[it.key()]->TasksDelete(it.value());
    }
}
