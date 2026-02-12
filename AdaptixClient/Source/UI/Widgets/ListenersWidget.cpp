#include <QJSEngine>
#include <UI/Widgets/ListenersWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <UI/Dialogs/DialogListener.h>
#include <UI/Dialogs/DialogAgent.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/Requestor.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <Client/AxScript/AxScriptManager.h>

REGISTER_DOCK_WIDGET(ListenersWidget, "Listeners", true)

ListenersWidget::ListenersWidget(AdaptixWidget* w) : DockTab("Listeners", w->GetProfile()->GetProject(), ":/icons/listeners"), adaptixWidget(w)
{
    this->createUI();

    connect(tableView, &QTableView::doubleClicked,              this, &ListenersWidget::onEditListener);
    connect(tableView, &QTableView::customContextMenuRequested, this, &ListenersWidget::handleListenersMenu);
    connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &selected, const QItemSelection &deselected){
            Q_UNUSED(selected)
            Q_UNUSED(deselected)
            tableView->setFocus();
    });
    connect(inputFilter, &QLineEdit::textChanged,   this, &ListenersWidget::onFilterUpdate);
    connect(inputFilter, &QLineEdit::returnPressed, this, [this]() { proxyModel->setTextFilter(inputFilter->text()); });
    connect(hideButton,  &ClickableLabel::clicked,  this, &ListenersWidget::toggleSearchPanel);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), this);
    shortcutSearch->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &ListenersWidget::toggleSearchPanel);

    auto shortcutEsc = new QShortcut(QKeySequence(Qt::Key_Escape), inputFilter);
    shortcutEsc->setContext(Qt::WidgetShortcut);
    connect(shortcutEsc, &QShortcut::activated, this, [this]() { searchWidget->setVisible(false); });

    this->dockWidget->setWidget(this);
}

ListenersWidget::~ListenersWidget() = default;

void ListenersWidget::SetUpdatesEnabled(const bool enabled)
{
    if (proxyModel)
        proxyModel->setDynamicSortFilter(enabled);
    if (tableView)
        tableView->setSortingEnabled(enabled);

    tableView->setUpdatesEnabled(enabled);
}

void ListenersWidget::createUI()
{
    auto horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    searchWidget = new QWidget(this);
    searchWidget->setVisible(false);

    inputFilter = new QLineEdit(searchWidget);
    inputFilter->setPlaceholderText("filter: (http | https) & ^(test)");
    inputFilter->setMaximumWidth(300);

    autoSearchCheck = new QCheckBox("auto", searchWidget);
    autoSearchCheck->setChecked(true);
    autoSearchCheck->setToolTip("Auto search on text change. If unchecked, press Enter to search.");

    hideButton = new ClickableLabel("  x  ");
    hideButton->setCursor(Qt::PointingHandCursor);
    hideButton->setStyleSheet("QLabel { color: #888; font-weight: bold; } QLabel:hover { color: #e34234; }");

    searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(0, 4, 0, 0);
    searchLayout->setSpacing(4);
    searchLayout->addWidget(inputFilter);
    searchLayout->addWidget(autoSearchCheck);
    searchLayout->addSpacing(8);
    searchLayout->addWidget(hideButton);
    searchLayout->addSpacerItem(horizontalSpacer);

    listenersModel = new ListenersTableModel(this);
    proxyModel = new ListenersFilterProxyModel(this);
    proxyModel->setSourceModel(listenersModel);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    tableView = new QTableView(this);
    tableView->setModel(proxyModel);
    tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    tableView->setAutoFillBackground(false);
    tableView->setShowGrid(false);
    tableView->setSortingEnabled(true);
    tableView->setWordWrap(true);
    tableView->setCornerButtonEnabled(false);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView->setFocusPolicy(Qt::NoFocus);
    tableView->setAlternatingRowColors(true);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableView->horizontalHeader()->setCascadingSectionResizes(true);
    tableView->horizontalHeader()->setHighlightSections(false);
    tableView->verticalHeader()->setVisible(false);

    tableView->setItemDelegate(new PaddingDelegate(tableView));
    tableView->sortByColumn(LC_Date, Qt::AscendingOrder);

    mainGridLayout = new QGridLayout(this);
    mainGridLayout->setContentsMargins(0, 0, 0, 0);
    mainGridLayout->addWidget(searchWidget, 0, 0, 1, 1);
    mainGridLayout->addWidget(tableView, 1, 0, 1, 1);
}

void ListenersWidget::Clear() const
{
    adaptixWidget->Listeners.clear();
    listenersModel->clear();
    inputFilter->clear();
}

void ListenersWidget::AddListenerItem(const ListenerData &newListener) const
{
    if (listenersModel->contains(newListener.Name))
        return;

    listenersModel->add(newListener);
    adaptixWidget->Listeners.push_back(newListener);

    tableView->horizontalHeader()->setSectionResizeMode(LC_Name,     QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(LC_RegName,  QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(LC_Type,     QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(LC_Protocol, QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(LC_BindHost, QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(LC_BindPort, QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(LC_Date,     QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(LC_Status,   QHeaderView::ResizeToContents);
}

void ListenersWidget::EditListenerItem(const ListenerData &newListener) const
{
    for (int i = 0; i < adaptixWidget->Listeners.size(); i++) {
        if (adaptixWidget->Listeners[i].Name == newListener.Name) {
            adaptixWidget->Listeners[i].BindHost = newListener.BindHost;
            adaptixWidget->Listeners[i].BindPort = newListener.BindPort;
            adaptixWidget->Listeners[i].AgentAddresses = newListener.AgentAddresses;
            adaptixWidget->Listeners[i].Status = newListener.Status;
            adaptixWidget->Listeners[i].Data = newListener.Data;
            break;
        }
    }

    listenersModel->update(newListener.Name, newListener);
}

void ListenersWidget::RemoveListenerItem(const QString &listenerName) const
{
    for (int i = 0; i < adaptixWidget->Listeners.size(); i++) {
        if (adaptixWidget->Listeners[i].Name == listenerName) {
            adaptixWidget->Listeners.erase(adaptixWidget->Listeners.begin() + i);
            break;
        }
    }

    listenersModel->remove(listenerName);
}

/// Slots

void ListenersWidget::toggleSearchPanel() const
{
    if (searchWidget->isVisible()) {
        searchWidget->setVisible(false);
    } else {
        searchWidget->setVisible(true);
        inputFilter->setFocus();
    }
}

void ListenersWidget::onFilterUpdate() const
{
    if (autoSearchCheck->isChecked()) {
        proxyModel->setTextFilter(inputFilter->text());
    }
    inputFilter->setFocus();
}

void ListenersWidget::handleListenersMenu(const QPoint &pos) const
{
    QMenu listenerMenu = QMenu();

    listenerMenu.addAction("Create", this, &ListenersWidget::onCreateListener);
    listenerMenu.addAction("Edit",   this, &ListenersWidget::onEditListener);
    listenerMenu.addAction("Remove", this, &ListenersWidget::onRemoveListener);
    listenerMenu.addSeparator();
    listenerMenu.addAction("Pause",  this, &ListenersWidget::onPauseListener);
    listenerMenu.addAction("Resume", this, &ListenersWidget::onResumeListener);
    listenerMenu.addSeparator();
    listenerMenu.addAction("Generate agent", this, &ListenersWidget::onGenerateAgent);

    QPoint globalPos = tableView->mapToGlobal(pos);
    listenerMenu.exec(globalPos);
}

void ListenersWidget::onCreateListener() const
{
    QList<RegListenerConfig> listeners;
    QMap<QString, AxUI> ax_uis;

    auto listenersList = adaptixWidget->ScriptManager->ListenerScriptList();

    for (auto listener : listenersList) {
        auto engine = adaptixWidget->ScriptManager->ListenerScriptEngine(listener);
        if (engine == nullptr) {
            adaptixWidget->ScriptManager->consolePrintError(QString("Listener %1 is not registered").arg(listener));
            continue;
        }

        QJSValue func = engine->globalObject().property("ListenerUI");
        if (!func.isCallable()) {
            adaptixWidget->ScriptManager->consolePrintError(listener + " - function ListenerUI is not registered");
            continue;
        }

        QJSValueList args;
        args << QJSValue(true);
        QJSValue result = func.call(args);
        if (result.isError()) {
            QString error = QStringLiteral("%1\n  at line %2 in %3\n  stack: %4").arg(result.toString()).arg(result.property("lineNumber").toInt()).arg(listener).arg(result.property("stack").toString());
            adaptixWidget->ScriptManager->consolePrintError(error);
            continue;
        }

        if (!result.isObject()) {
            adaptixWidget->ScriptManager->consolePrintError(listener + " - function ListenerUI must return panel and container objects");
            continue;
        }

        QJSValue ui_container = result.property("ui_container");
        QJSValue ui_panel     = result.property("ui_panel");
        QJSValue ui_height    = result.property("ui_height");
        QJSValue ui_width     = result.property("ui_width");

        if ( ui_container.isUndefined() || !ui_container.isObject() || ui_panel.isUndefined() || !ui_panel.isQObject()) {
            adaptixWidget->ScriptManager->consolePrintError(listener + " - function ListenerUI must return panel and container objects");
            continue;
        }

        QObject* objPanel = ui_panel.toQObject();
        auto* formElement = dynamic_cast<AxPanelWrapper*>(objPanel);
        if (!formElement) {
            adaptixWidget->ScriptManager->consolePrintError(listener + " - function ListenerUI must return panel and container objects");
            continue;
        }

        QObject* objContainer = ui_container.toQObject();
        auto* container = dynamic_cast<AxContainerWrapper*>(objContainer);
        if (!container) {
            adaptixWidget->ScriptManager->consolePrintError(listener + " - function ListenerUI must return panel and container objects");
            continue;
        }

        int h = 650;
        if (ui_height.isNumber() && ui_height.toInt() > 0) {
            h = ui_height.toInt();
        }

        int w = 650;
        if (ui_width.isNumber() && ui_width.toInt() > 0) {
            w = ui_width.toInt();
        }

        auto regListener = adaptixWidget->GetRegListener(listener);

        listeners.append(regListener);
        ax_uis[listener] = { container, formElement->widget(), h, w };
    }

    DialogListener* dialogListener = new DialogListener();
    dialogListener->setAttribute(Qt::WA_DeleteOnClose);
    dialogListener->SetProfile( *(adaptixWidget->GetProfile()) );
    dialogListener->AddExListeners(listeners, ax_uis);
    dialogListener->Start();
}

void ListenersWidget::onEditListener() const
{
    if (tableView->selectionModel()->selectedRows().empty())
        return;

    QModelIndex currentIndex = tableView->currentIndex();
    QModelIndex sourceIndex = proxyModel->mapToSource(currentIndex);
    if (!sourceIndex.isValid())
        return;

    auto listenerName    = listenersModel->data(listenersModel->index(sourceIndex.row(), LC_Name), Qt::DisplayRole).toString();
    auto listenerRegName = listenersModel->data(listenersModel->index(sourceIndex.row(), LC_RegName), Qt::DisplayRole).toString();

    QString listenerData = "";
    for (auto listener : adaptixWidget->Listeners) {
        if(listener.Name == listenerName) {
            listenerData = listener.Data;
            break;
        }
    }

    QList<RegListenerConfig> listeners;
    QMap<QString, AxUI> ax_uis;

    auto engine = adaptixWidget->ScriptManager->ListenerScriptEngine(listenerRegName);
    if (engine == nullptr) {
        adaptixWidget->ScriptManager->consolePrintError(QString("Listener %1 is not registered").arg(listenerName));
        return;;
    }

    QJSValue func = engine->globalObject().property("ListenerUI");
    if (!func.isCallable()) {
        adaptixWidget->ScriptManager->consolePrintError(listenerName + " - function ListenerUI is not registered");
        return;
    }

    QJSValueList args;
    args << QJSValue(false);
    QJSValue result = func.call(args);
    if (result.isError()) {
        QString error = QStringLiteral("%1\n  at line %2 in %3\n  stack: %4").arg(result.toString()).arg(result.property("lineNumber").toInt()).arg(listenerName).arg(result.property("stack").toString());
        adaptixWidget->ScriptManager->consolePrintError(error);
        return;
    }

    if (!result.isObject()) {
        adaptixWidget->ScriptManager->consolePrintError(listenerName + " - function ListenerUI must return panel and container objects");
        return;
    }

    QJSValue ui_container = result.property("ui_container");
    QJSValue ui_panel     = result.property("ui_panel");
    QJSValue ui_height    = result.property("ui_height");
    QJSValue ui_width     = result.property("ui_width");

    if ( ui_container.isUndefined() || !ui_container.isObject() || ui_panel.isUndefined() || !ui_panel.isQObject()) {
        adaptixWidget->ScriptManager->consolePrintError(listenerName + " - function ListenerUI must return panel and container objects");
        return;
    }

    QObject* objPanel = ui_panel.toQObject();
    auto* formElement = dynamic_cast<AxPanelWrapper*>(objPanel);
    if (!formElement) {
        adaptixWidget->ScriptManager->consolePrintError(listenerName + " - function ListenerUI must return panel and container objects");
        return;
    }

    QObject* objContainer = ui_container.toQObject();
    auto* container = dynamic_cast<AxContainerWrapper*>(objContainer);
    if (!container) {
        adaptixWidget->ScriptManager->consolePrintError(listenerName + " - function ListenerUI must return panel and container objects");
        return;
    }

    int h = 650;
    if (ui_height.isNumber() && ui_height.toInt() > 0) {
        h = ui_height.toInt();
    }

    int w = 650;
    if (ui_width.isNumber() && ui_width.toInt() > 0) {
        w = ui_width.toInt();
    }

    listeners.append(adaptixWidget->GetRegListener(listenerRegName));
    ax_uis[listenerRegName] = { container, formElement->widget(), h, w };

    container->fromJson(listenerData);

    DialogListener* dialogListener = new DialogListener();
    dialogListener->setAttribute(Qt::WA_DeleteOnClose);
    dialogListener->SetEditMode(listenerName);
    dialogListener->SetProfile( *(adaptixWidget->GetProfile()) );
    dialogListener->AddExListeners(listeners, ax_uis);
    dialogListener->Start();
}

void ListenersWidget::onRemoveListener() const
{
    if (tableView->selectionModel()->selectedRows().empty())
        return;

    QModelIndex currentIndex = tableView->currentIndex();
    QModelIndex sourceIndex = proxyModel->mapToSource(currentIndex);
    if (!sourceIndex.isValid())
        return;

    auto listenerName    = listenersModel->data(listenersModel->index(sourceIndex.row(), LC_Name), Qt::DisplayRole).toString();
    auto listenerRegName = listenersModel->data(listenersModel->index(sourceIndex.row(), LC_RegName), Qt::DisplayRole).toString();

    QMessageBox::StandardButton reply = QMessageBox::question(nullptr, "Delete Confirmation",
                                      QString("Are you sure you want to remove the '%1' listener?").arg(listenerName),
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    HttpReqListenerStopAsync(listenerName, listenerRegName, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void ListenersWidget::onPauseListener() const
{
    if (tableView->selectionModel()->selectedRows().empty())
        return;

    QModelIndex currentIndex = tableView->currentIndex();
    QModelIndex sourceIndex = proxyModel->mapToSource(currentIndex);
    if (!sourceIndex.isValid())
        return;

    auto listenerName    = listenersModel->data(listenersModel->index(sourceIndex.row(), LC_Name), Qt::DisplayRole).toString();
    auto listenerRegName = listenersModel->data(listenersModel->index(sourceIndex.row(), LC_RegName), Qt::DisplayRole).toString();

    HttpReqListenerPauseAsync(listenerName, listenerRegName, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void ListenersWidget::onResumeListener() const
{
    if (tableView->selectionModel()->selectedRows().empty())
        return;

    QModelIndex currentIndex = tableView->currentIndex();
    QModelIndex sourceIndex = proxyModel->mapToSource(currentIndex);
    if (!sourceIndex.isValid())
        return;

    auto listenerName    = listenersModel->data(listenersModel->index(sourceIndex.row(), LC_Name), Qt::DisplayRole).toString();
    auto listenerRegName = listenersModel->data(listenersModel->index(sourceIndex.row(), LC_RegName), Qt::DisplayRole).toString();

    HttpReqListenerResumeAsync(listenerName, listenerRegName, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void ListenersWidget::onGenerateAgent() const
{
    if (tableView->selectionModel()->selectedRows().empty())
        return;

    QModelIndex currentIndex = tableView->currentIndex();
    QModelIndex sourceIndex = proxyModel->mapToSource(currentIndex);
    if (!sourceIndex.isValid())
        return;

    auto listenerName    = listenersModel->data(listenersModel->index(sourceIndex.row(), LC_Name), Qt::DisplayRole).toString();
    auto listenerRegName = listenersModel->data(listenersModel->index(sourceIndex.row(), LC_RegName), Qt::DisplayRole).toString();

    QList<QString> agentNames = adaptixWidget->GetAgentNames(listenerRegName);

    QStringList agents;
    QMap<QString, AxUI> ax_uis;

    for (auto agent : agentNames) {
        auto engine = adaptixWidget->ScriptManager->AgentScriptEngine(agent);
        if (engine == nullptr) {
            adaptixWidget->ScriptManager->consolePrintError(QString("Listener %1 is not registered").arg(agent));
            return;;
        }

        QJSValue func = engine->globalObject().property("GenerateUI");
        if (!func.isCallable()) {
            adaptixWidget->ScriptManager->consolePrintError(listenerName + " - function GenerateUI is not registered");
            return;
        }

        QJSValue jsListeners = engine->newArray(1);
        jsListeners.setProperty(0, listenerRegName);

        QJSValueList args;
        args << jsListeners;
        QJSValue result = func.call(args);
        if (result.isError()) {
            QString error = QStringLiteral("%1\n  at line %2 in %3\n  stack: %4").arg(result.toString()).arg(result.property("lineNumber").toInt()).arg(listenerName).arg(result.property("stack").toString());
            adaptixWidget->ScriptManager->consolePrintError(error);
            return;
        }

        if (!result.isObject()) {
            adaptixWidget->ScriptManager->consolePrintError(listenerName + " - function GenerateUI must return panel and container objects");
            return;
        }

        QJSValue ui_container = result.property("ui_container");
        QJSValue ui_panel     = result.property("ui_panel");
        QJSValue ui_height    = result.property("ui_height");
        QJSValue ui_width     = result.property("ui_width");

        if ( ui_container.isUndefined() || !ui_container.isObject() || ui_panel.isUndefined() || !ui_panel.isQObject()) {
            adaptixWidget->ScriptManager->consolePrintError(listenerName + " - function GenerateUI must return panel and container objects");
            return;
        }

        QObject* objPanel = ui_panel.toQObject();
        auto* formElement = dynamic_cast<AxPanelWrapper*>(objPanel);
        if (!formElement) {
            adaptixWidget->ScriptManager->consolePrintError(listenerName + " - function GenerateUI must return panel and container objects");
            return;
        }

        QObject* objContainer = ui_container.toQObject();
        auto* container = dynamic_cast<AxContainerWrapper*>(objContainer);
        if (!container) {
            adaptixWidget->ScriptManager->consolePrintError(listenerName + " - function GenerateUI must return panel and container objects");
            return;
        }

        int h = 550;
        if (ui_height.isNumber() && ui_height.toInt() > 0) {
            h = ui_height.toInt();
        }

        int w = 550;
        if (ui_width.isNumber() && ui_width.toInt() > 0) {
            w = ui_width.toInt();
        }

        agents.append(agent);
        ax_uis[agent] = { container, formElement->widget(), h, w };
    }

    QMap<QString, AgentTypeInfo> agentTypesMap;
    for (const auto &agentItem : agents) {
        agentTypesMap[agentItem] = adaptixWidget->GetAgentTypeInfo(agentItem);
    }

    DialogAgent* dialogListener = new DialogAgent(adaptixWidget, listenerName, listenerRegName);
    dialogListener->setAttribute(Qt::WA_DeleteOnClose);
    dialogListener->SetProfile( *(adaptixWidget->GetProfile()) );
    dialogListener->SetAvailableListeners(adaptixWidget->Listeners);
    dialogListener->SetAgentTypes(agentTypesMap);
    dialogListener->AddExAgents(agents, ax_uis);
    dialogListener->Start();
}
