#include <Agent/Agent.h>
#include <UI/Widgets/SessionsTableWidget.h>
#include <UI/Widgets/BrowserFilesWidget.h>
#include <UI/Widgets/BrowserProcessWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/TerminalContainerWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/TasksWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <UI/Dialogs/DialogTunnel.h>
#include <UI/Dialogs/DialogAgentData.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/Requestor.h>
#include <Client/Settings.h>
#include <Client/TunnelEndpoint.h>
#include <Client/AuthProfile.h>
#include <MainAdaptix.h>

REGISTER_DOCK_WIDGET(SessionsTableWidget, "Sessions", true)

SessionsTableWidget::SessionsTableWidget( AdaptixWidget* w ) : DockTab("Sessions table", w->GetProfile()->GetProject(), ":/icons/format_list")
{
    this->adaptixWidget = w;

    this->createUI();

    connect( tableView, &QTableWidget::doubleClicked,              this, &SessionsTableWidget::handleTableDoubleClicked );
    connect( tableView, &QTableWidget::customContextMenuRequested, this, &SessionsTableWidget::handleSessionsTableMenu );
    connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &selected, const QItemSelection &deselected){
        Q_UNUSED(selected)
        Q_UNUSED(deselected)
        tableView->setFocus();
    });

    connect(inputFilter,    &QLineEdit::textChanged,        this, &SessionsTableWidget::onFilterChanged);
    connect(inputFilter,    &QLineEdit::returnPressed,      this, [this]() { proxyModel->setTextFilter(inputFilter->text()); });
    connect(comboAgentType, &QComboBox::currentTextChanged, this, &SessionsTableWidget::onFilterChanged);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(checkOnlyActive, &QCheckBox::checkStateChanged, this, &SessionsTableWidget::onFilterChanged);
#else
    connect(checkOnlyActive, &QCheckBox::stateChanged, this, &SessionsTableWidget::onFilterChanged);
#endif
    connect(hideButton, &ClickableLabel::clicked, this, &SessionsTableWidget::toggleSearchPanel);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), this);
    shortcutSearch->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &SessionsTableWidget::toggleSearchPanel);

    auto shortcutEsc = new QShortcut(QKeySequence(Qt::Key_Escape), inputFilter);
    shortcutEsc->setContext(Qt::WidgetShortcut);
    connect(shortcutEsc, &QShortcut::activated, this, [this]() { searchWidget->setVisible(false); });

    this->dockWidget->setWidget(this);
}

SessionsTableWidget::~SessionsTableWidget() = default;

void SessionsTableWidget::SetUpdatesEnabled(const bool enabled)
{
    tableView->setUpdatesEnabled(enabled);
}

void SessionsTableWidget::createUI()
{
    auto horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    searchWidget = new QWidget(this);
    searchWidget->setVisible(false);

    inputFilter = new QLineEdit(searchWidget);
    inputFilter->setPlaceholderText("filter: (admin | root) & ^(test)");
    inputFilter->setMaximumWidth(300);

    autoSearchCheck = new QCheckBox("auto", searchWidget);
    autoSearchCheck->setChecked(true);
    autoSearchCheck->setToolTip("Auto search on text change. If unchecked, press Enter to search.");

    comboAgentType = new QComboBox(searchWidget);
    comboAgentType->setMinimumWidth(150);
    comboAgentType->addItem("All types");

    checkOnlyActive = new QCheckBox("only active", searchWidget);

    hideButton = new ClickableLabel("  x  ");
    hideButton->setCursor(Qt::PointingHandCursor);
    hideButton->setStyleSheet("QLabel { color: #888; font-weight: bold; } QLabel:hover { color: #e34234; }");

    searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(0, 4, 0, 0);
    searchLayout->setSpacing(4);
    searchLayout->addWidget(inputFilter);
    searchLayout->addWidget(autoSearchCheck);
    searchLayout->addSpacing(8);
    searchLayout->addWidget(comboAgentType);
    searchLayout->addSpacing(8);
    searchLayout->addWidget(checkOnlyActive);
    searchLayout->addSpacing(8);
    searchLayout->addWidget(hideButton);
    searchLayout->addSpacerItem(horizontalSpacer);

    agentsModel = new AgentsTableModel(adaptixWidget, this);
    proxyModel  = new AgentsFilterProxyModel(adaptixWidget, this);
    proxyModel->setSourceModel(agentsModel);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    tableView = new QTableView( this );
    tableView->setModel(proxyModel);
    tableView->setContextMenuPolicy( Qt::CustomContextMenu );
    tableView->setAutoFillBackground( false );
    tableView->setShowGrid( false );
    tableView->setSortingEnabled( true );
    tableView->setWordWrap( false );
    tableView->setCornerButtonEnabled( false );
    tableView->setSelectionBehavior( QAbstractItemView::SelectRows );
    tableView->setFocusPolicy( Qt::NoFocus );
    tableView->setAlternatingRowColors( true );
    tableView->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    tableView->horizontalHeader()->setCascadingSectionResizes( true );
    tableView->horizontalHeader()->setHighlightSections( false );
    tableView->verticalHeader()->setVisible( false );

    proxyModel->sort(-1);

    tableView->setItemDelegate(new PaddingDelegate(tableView));

    this->UpdateColumnsVisible();

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->setContentsMargins( 0, 0,  0, 0);
    mainGridLayout->addWidget( searchWidget, 0, 0, 1, 1);
    mainGridLayout->addWidget( tableView,    1, 0, 1, 1);
}

void SessionsTableWidget::AddAgentItem( Agent* newAgent ) const
{
    if ( adaptixWidget->AgentsMap.contains(newAgent->data.Id) )
        return;

    adaptixWidget->AgentsMap[ newAgent->data.Id ] = newAgent;

    agentsModel->add(newAgent->data.Id);

    if (adaptixWidget->IsSynchronized())
        this->UpdateColumnsSize();
}

void SessionsTableWidget::UpdateAgentItem(const AgentData &oldDatam, const Agent* agent) const
{
    agentsModel->update(agent->data.Id);

    if (oldDatam.Username != agent->data.Username || oldDatam.Impersonated != agent->data.Impersonated )
        this->UpdateColumnsSize();
}

void SessionsTableWidget::RemoveAgentItem(const QString &agentId) const
{
    if (!adaptixWidget->AgentsMap.contains(agentId))
        return;

    Agent* agent = adaptixWidget->AgentsMap[agentId];
    adaptixWidget->AgentsMap.remove(agentId);
    delete agent;

    agentsModel->remove(agentId);
}

void SessionsTableWidget::UpdateColumnsVisible() const
{
    for (int i = 0; i < SC_ColumnCount; i++) {
        if (GlobalClient->settings->data.SessionsTableColumns[i])
            tableView->showColumn(i);
        else
            tableView->hideColumn(i);
    }
}

void SessionsTableWidget::UpdateColumnsSize() const
{
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(SC_Tags, QHeaderView::Stretch);

    int wDomain   = tableView->columnWidth(SC_Domain);
    int wComputer = tableView->columnWidth(SC_Computer);
    int wUser     = tableView->columnWidth(SC_User);
    int wOs       = tableView->columnWidth(SC_Os);
    int wProcess  = tableView->columnWidth(SC_Process);

    tableView->horizontalHeader()->setSectionResizeMode(SC_Domain,   QHeaderView::Interactive);
    tableView->horizontalHeader()->setSectionResizeMode(SC_Computer, QHeaderView::Interactive);
    tableView->horizontalHeader()->setSectionResizeMode(SC_User,     QHeaderView::Interactive);
    tableView->horizontalHeader()->setSectionResizeMode(SC_Os,       QHeaderView::Interactive);
    tableView->horizontalHeader()->setSectionResizeMode(SC_Process,  QHeaderView::Interactive);

    tableView->setColumnWidth(SC_Domain,   wDomain);
    tableView->setColumnWidth(SC_Computer, wComputer);
    tableView->setColumnWidth(SC_User,     wUser);
    tableView->setColumnWidth(SC_Os,       wOs);
    tableView->setColumnWidth(SC_Process,  wProcess);
}

void SessionsTableWidget::UpdateData() const
{
    auto f = qobject_cast<AgentsFilterProxyModel*>(proxyModel);
    f->updateVisible();
}

void SessionsTableWidget::UpdateAgentTypeComboBox() const
{
    QSet<QString> types;
    for (const auto& agent : adaptixWidget->AgentsMap) {
        if (agent && !agent->data.Name.isEmpty())
            types.insert(agent->data.Name);
    }

    QString currentType = comboAgentType->currentText();

    comboAgentType->blockSignals(true);
    comboAgentType->clear();
    comboAgentType->addItem("All types");
    QStringList typeList = types.values();
    typeList.sort();
    comboAgentType->addItems(typeList);

    int idx = comboAgentType->findText(currentType);
    if (idx >= 0)
        comboAgentType->setCurrentIndex(idx);
    comboAgentType->blockSignals(false);
}

void SessionsTableWidget::Clear() const
{
    for (auto agentId : adaptixWidget->AgentsMap.keys()) {
        Agent* agent = adaptixWidget->AgentsMap[agentId];
        adaptixWidget->AgentsMap.remove(agentId);
        delete agent;
    }

    agentsModel->clear();

    checkOnlyActive->setChecked(false);
    inputFilter->clear();
    comboAgentType->clear();
    comboAgentType->addItem("All types");
}



/// SLOTS

void SessionsTableWidget::toggleSearchPanel() const
{
    if (this->searchWidget->isVisible()) {
        this->searchWidget->setVisible(false);
        proxyModel->setSearchVisible(false);
    }
    else {
        this->searchWidget->setVisible(true);
        proxyModel->setSearchVisible(true);
    }
}

void SessionsTableWidget::handleTableDoubleClicked(const QModelIndex &index) const
{
    auto idx = tableView->currentIndex();
    if (!idx.isValid())
        return;

    QString AgentId = proxyModel->index(idx.row(), SC_AgentID).data().toString();
    if (AgentId.isEmpty())
        return;

    adaptixWidget->LoadConsoleUI(AgentId);
}

void SessionsTableWidget::onFilterChanged() const
{
    if (autoSearchCheck->isChecked()) {
        proxyModel->setTextFilter(inputFilter->text());
    }
    proxyModel->setOnlyActive(checkOnlyActive->isChecked());

    QSet<QString> selectedTypes;
    QString currentType = comboAgentType->currentText();
    if (currentType != "All types" && !currentType.isEmpty()) {
        selectedTypes.insert(currentType);
    }
    proxyModel->setAgentTypes(selectedTypes);
}

/// Menu

void SessionsTableWidget::handleSessionsTableMenu(const QPoint &pos)
{
    QMenu ctxMenu;

    QModelIndex index = tableView->indexAt(pos);
    if (index.isValid()) {
        QStringList agentIds;
        QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
        for (const QModelIndex &proxyIndex : selectedRows) {
            QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
            if (!sourceIndex.isValid()) continue;

            QString agentId = agentsModel->data(agentsModel->index(sourceIndex.row(), SC_AgentID), Qt::DisplayRole).toString();
            agentIds.append(agentId);
        }

        auto agentMenu = ctxMenu.addMenu("Agent");
        agentMenu->addAction("Execute command", this, &SessionsTableWidget::actionExecuteCommand);
        agentMenu->addAction("Task manager", this, &SessionsTableWidget::actionTasksBrowserOpen);
        agentMenu->addSeparator();

        int agentCount = adaptixWidget->ScriptManager->AddMenuSession(agentMenu, "SessionAgent", agentIds);
        if (agentCount > 0)
            agentMenu->addSeparator();

        agentMenu->addAction("Remove console data", this, &SessionsTableWidget::actionConsoleDelete);
        agentMenu->addAction("Remove from server", this, &SessionsTableWidget::actionAgentRemove);

        auto sessionMenu = ctxMenu.addMenu("Session");
        sessionMenu->addAction("Mark as Active",   this, &SessionsTableWidget::actionMarkActive);
        sessionMenu->addAction("Mark as Inactive", this, &SessionsTableWidget::actionMarkInactive);
        sessionMenu->addSeparator();
        if ( agentIds.size() == 1 )
            sessionMenu->addAction("Set data", this, &SessionsTableWidget::actionSetData);
        sessionMenu->addAction("Set tag", this, &SessionsTableWidget::actionItemTag);
        sessionMenu->addSeparator();
        sessionMenu->addAction("Set items color", this, &SessionsTableWidget::actionItemColor);
        sessionMenu->addAction("Set text color",  this, &SessionsTableWidget::actionTextColor);
        sessionMenu->addAction("Reset color",     this, &SessionsTableWidget::actionColorReset);
        sessionMenu->addSeparator();
        sessionMenu->addAction("Hide on client", this, &SessionsTableWidget::actionItemHide);

        ctxMenu.addAction("Console", this, &SessionsTableWidget::actionConsoleOpen);
        ctxMenu.addSeparator();
        ctxMenu.addMenu(agentMenu);

        auto browserMenu = ctxMenu.addMenu("Browsers");
        int browserCount = adaptixWidget->ScriptManager->AddMenuSession(browserMenu, "SessionBrowser", agentIds);
        if (browserCount > 0)
            ctxMenu.addMenu(browserMenu);
        else
            ctxMenu.removeAction(browserMenu->menuAction());

        auto accessMenu = ctxMenu.addMenu("Access");
        int accessCount = adaptixWidget->ScriptManager->AddMenuSession(accessMenu, "SessionAccess", agentIds);
        if (accessCount > 0)
            ctxMenu.addMenu(accessMenu);
        else
            ctxMenu.removeAction(accessMenu->menuAction());

        adaptixWidget->ScriptManager->AddMenuSession(&ctxMenu, "SessionMain", agentIds);

        ctxMenu.addSeparator();
        ctxMenu.addMenu(sessionMenu);
    }
    ctxMenu.addAction("Show all items", this, &SessionsTableWidget::actionItemsShowAll);

    ctxMenu.exec(tableView->viewport()->mapToGlobal(pos));
}

void SessionsTableWidget::actionConsoleOpen() const
{
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString agentId = agentsModel->data(agentsModel->index(sourceIndex.row(), SC_AgentID), Qt::DisplayRole).toString();
        adaptixWidget->LoadConsoleUI(agentId);
    }
}

void SessionsTableWidget::actionExecuteCommand()
{
    QStringList listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString agentId = agentsModel->data(agentsModel->index(sourceIndex.row(), SC_AgentID), Qt::DisplayRole).toString();
        listId.append(agentId);
    }

    if(listId.empty())
        return;

    bool ok = false;
    QString cmd = QInputDialog::getText(this,"Execute Command", "Command", QLineEdit::Normal, "", &ok);
    if (!ok)
        return;

    for(auto id : listId) {
        adaptixWidget->AgentsMap[id]->Console->SetInput(cmd);
        adaptixWidget->AgentsMap[id]->Console->processInput();
    }
}

void SessionsTableWidget::actionTasksBrowserOpen() const
{
    auto idx = tableView->currentIndex();
    if (idx.isValid()) {
        QString agentId = proxyModel->index(idx.row(), SC_AgentID).data().toString();

        adaptixWidget->TasksDock->SetAgentFilter(agentId);
        adaptixWidget->SetTasksUI();
    }
}

void SessionsTableWidget::actionMarkActive() const
{
    QStringList listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString agentId = agentsModel->data(agentsModel->index(sourceIndex.row(), SC_AgentID), Qt::DisplayRole).toString();
        listId.append(agentId);
    }

    if(listId.empty())
        return;

    HttpReqAgentSetMarkAsync(listId, "", *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void SessionsTableWidget::actionMarkInactive() const
{
    QStringList listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString agentId = agentsModel->data(agentsModel->index(sourceIndex.row(), SC_AgentID), Qt::DisplayRole).toString();
        listId.append(agentId);
    }

    if(listId.empty())
        return;

    HttpReqAgentSetMarkAsync(listId, "Inactive", *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void SessionsTableWidget::actionItemColor() const
{
    QStringList listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString agentId = agentsModel->data(agentsModel->index(sourceIndex.row(), SC_AgentID), Qt::DisplayRole).toString();
        listId.append(agentId);
    }

    if(listId.empty())
        return;

    QColor itemColor = QColorDialog::getColor(Qt::white, nullptr, "Select items color");
    if (itemColor.isValid()) {
        QString itemColorHex = itemColor.name();
        HttpReqAgentSetColorAsync(listId, itemColorHex, "", false, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void SessionsTableWidget::actionTextColor() const
{
    QStringList listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString agentId = agentsModel->data(agentsModel->index(sourceIndex.row(), SC_AgentID), Qt::DisplayRole).toString();
        listId.append(agentId);
    }

    if(listId.empty())
        return;

    QColor textColor = QColorDialog::getColor(Qt::white, nullptr, "Select text color");
    if (textColor.isValid()) {
        QString textColorHex = textColor.name();
        HttpReqAgentSetColorAsync(listId, "", textColorHex, false, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void SessionsTableWidget::actionColorReset() const
{
    QStringList listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString agentId = agentsModel->data(agentsModel->index(sourceIndex.row(), SC_AgentID), Qt::DisplayRole).toString();
        listId.append(agentId);
    }

    if(listId.empty())
        return;

    HttpReqAgentSetColorAsync(listId, "", "", true, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void SessionsTableWidget::actionConsoleDelete()
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Clear Confirmation",
                                      "Are you sure you want to delete all agent console data and history from server (tasks will not be deleted from TaskManager)?\n\n"
                                      "If you want to temporarily hide the contents of the agent console, do so through the agent console menu.",
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    QStringList listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString agentId = agentsModel->data(agentsModel->index(sourceIndex.row(), SC_AgentID), Qt::DisplayRole).toString();
        listId.append(agentId);
    }

    if(listId.empty())
        return;

    for (auto id : listId) {
        adaptixWidget->AgentsMap[id]->Console->Clear();
    }

    HttpReqConsoleRemoveAsync(listId, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void SessionsTableWidget::actionAgentRemove()
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Confirmation",
                                      "Are you sure you want to delete all information about the selected agents from the server?\n\n"
                                      "If you want to hide the record, simply choose: 'Item -> Hide on Client'.",
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    QStringList listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString agentId = agentsModel->data(agentsModel->index(sourceIndex.row(), SC_AgentID), Qt::DisplayRole).toString();
        listId.append(agentId);
    }

    if(listId.empty())
        return;

    HttpReqAgentRemoveAsync(listId, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void SessionsTableWidget::actionItemTag() const
{
    QString tag = "";
    QStringList listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString cTag    = agentsModel->data(agentsModel->index(sourceIndex.row(), SC_Tags), Qt::DisplayRole).toString();
        QString agentId = agentsModel->data(agentsModel->index(sourceIndex.row(), SC_AgentID), Qt::DisplayRole).toString();
        listId.append(agentId);

        if (tag.isEmpty())
            tag = cTag;
    }

    if(listId.empty())
        return;

    bool inputOk;
    QString newTag = QInputDialog::getText(nullptr, "Set tags", "New tag", QLineEdit::Normal,tag, &inputOk);
    if ( inputOk ) {
        HttpReqAgentSetTagAsync(listId, newTag, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void SessionsTableWidget::actionItemHide() const
{
    bool refact = false;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString agentId = agentsModel->data(agentsModel->index(sourceIndex.row(), SC_AgentID), Qt::DisplayRole).toString();
        if (adaptixWidget->AgentsMap.contains(agentId)) {
            adaptixWidget->AgentsMap[agentId]->show = false;
            refact = true;
        }
    }

    if (refact) this->UpdateData();
}

void SessionsTableWidget::actionItemsShowAll() const
{
    bool refact = false;
    for (auto agent : adaptixWidget->AgentsMap) {
        if (agent->show == false) {
            agent->show = true;
            refact = true;
        }
    }

    if (refact) this->UpdateData();
}

void SessionsTableWidget::actionSetData() const
{
    auto idx = tableView->currentIndex();
    if (!idx.isValid())
        return;

    QModelIndex sourceIndex = proxyModel->mapToSource(idx);
    if (!sourceIndex.isValid())
        return;

    QString agentId = agentsModel->data(agentsModel->index(sourceIndex.row(), SC_AgentID), Qt::DisplayRole).toString();
    if (!adaptixWidget->AgentsMap.contains(agentId))
        return;

    Agent* agent = adaptixWidget->AgentsMap[agentId];

    auto* dialog = new DialogAgentData();
    dialog->SetProfile(*(adaptixWidget->GetProfile()));
    dialog->SetAgentData(agent->data);
    dialog->Start();
}