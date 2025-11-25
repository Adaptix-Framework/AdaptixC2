#include <QJSEngine>
#include <UI/Widgets/ListenersWidget.h>
#include <UI/Dialogs/DialogListener.h>
#include <UI/Dialogs/DialogAgent.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/Requestor.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <Client/AxScript/AxScriptManager.h>

ListenersWidget::ListenersWidget(AdaptixWidget* w) : DockTab("Listeners", w->GetProfile()->GetProject(), ":/icons/listeners"), adaptixWidget(w)
{
    this->createUI();

    connect(tableWidget, &QTableWidget::cellDoubleClicked,          this, &ListenersWidget::onEditListener);
    connect(tableWidget, &QTableWidget::customContextMenuRequested, this, &ListenersWidget::handleListenersMenu);

    this->dockWidget->setWidget(this);
}

ListenersWidget::~ListenersWidget() = default;

void ListenersWidget::SetUpdatesEnabled(const bool enabled)
{
    tableWidget->setUpdatesEnabled(enabled);
}

void ListenersWidget::createUI()
{
    tableWidget = new QTableWidget( this );
    tableWidget->setColumnCount( 8 );
    tableWidget->setContextMenuPolicy( Qt::CustomContextMenu );
    tableWidget->setAutoFillBackground( false );
    tableWidget->setShowGrid( false );
    tableWidget->setSortingEnabled( true );
    tableWidget->setWordWrap( true );
    tableWidget->setCornerButtonEnabled( false );
    tableWidget->setSelectionBehavior( QAbstractItemView::SelectRows );
    tableWidget->setSelectionMode( QAbstractItemView::SingleSelection );
    tableWidget->setFocusPolicy( Qt::NoFocus );
    tableWidget->setAlternatingRowColors( true );
    tableWidget->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    tableWidget->horizontalHeader()->setCascadingSectionResizes( true );
    tableWidget->horizontalHeader()->setHighlightSections( false );
    tableWidget->verticalHeader()->setVisible( false );

    tableWidget->setHorizontalHeaderItem( ColumnName,     new QTableWidgetItem( "Name" ) );
    tableWidget->setHorizontalHeaderItem( ColumnRegName,  new QTableWidgetItem( "Reg name" ) );
    tableWidget->setHorizontalHeaderItem( ColumnType,     new QTableWidgetItem( "Type" ) );
    tableWidget->setHorizontalHeaderItem( ColumnProtocol, new QTableWidgetItem( "Protocol" ) );
    tableWidget->setHorizontalHeaderItem( ColumnBindHost, new QTableWidgetItem( "Bind Host" ) );
    tableWidget->setHorizontalHeaderItem( ColumnBindPort, new QTableWidgetItem( "Bind Port" ) );
    tableWidget->setHorizontalHeaderItem( ColumnHosts,    new QTableWidgetItem( "C2 Hosts (agent)" ) );
    tableWidget->setHorizontalHeaderItem( ColumnStatus,   new QTableWidgetItem( "Status" ) );

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->setContentsMargins(0, 0,  0, 0);
    mainGridLayout->addWidget(tableWidget, 0, 0, 1, 1);
}

void ListenersWidget::Clear() const
{
    adaptixWidget->Listeners.clear();
    for (int index = tableWidget->rowCount(); index > 0; index-- )
        tableWidget->removeRow(index -1 );
}

void ListenersWidget::AddListenerItem(const ListenerData &newListener ) const
{
    for( auto listener : adaptixWidget->Listeners ) {
        if( listener.Name == newListener.Name )
            return;
    }

    auto item_Name      = new QTableWidgetItem( newListener.Name );
    auto item_RegName   = new QTableWidgetItem( newListener.ListenerRegName );
    auto item_Type      = new QTableWidgetItem( newListener.ListenerType );
    auto item_Protocol  = new QTableWidgetItem( newListener.ListenerProtocol );
    auto item_BindHost  = new QTableWidgetItem( newListener.BindHost );
    auto item_BindPort  = new QTableWidgetItem( newListener.BindPort );
    auto item_AgentHost = new QTableWidgetItem( newListener.AgentAddresses );
    auto item_Status    = new QTableWidgetItem( newListener.Status );

    item_Name->setFlags( item_Name->flags() ^ Qt::ItemIsEditable );

    item_RegName->setFlags( item_RegName->flags() ^ Qt::ItemIsEditable );

    item_Type->setFlags( item_Type->flags() ^ Qt::ItemIsEditable );
    item_Type->setTextAlignment( Qt::AlignCenter );

    item_Protocol->setFlags( item_Protocol->flags() ^ Qt::ItemIsEditable );
    item_Protocol->setTextAlignment( Qt::AlignCenter );

    item_BindHost->setFlags( item_BindHost->flags() ^ Qt::ItemIsEditable );
    item_BindHost->setTextAlignment( Qt::AlignCenter );

    item_BindPort->setFlags( item_BindPort->flags() ^ Qt::ItemIsEditable );
    item_BindPort->setTextAlignment( Qt::AlignCenter );

    item_AgentHost->setFlags( item_AgentHost->flags() ^ Qt::ItemIsEditable );
    item_AgentHost->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );

    item_Status->setFlags( item_Status->flags() ^ Qt::ItemIsEditable );
    item_Status->setTextAlignment( Qt::AlignCenter );
    if ( newListener.Status == "Listen"  )
        item_Status->setForeground(QColor(COLOR_NeonGreen));
    else
        item_Status->setForeground( QColor( COLOR_ChiliPepper ) );

    if( tableWidget->rowCount() < 1 )
        tableWidget->setRowCount( 1 );
    else
        tableWidget->setRowCount( tableWidget->rowCount() + 1 );

    bool isSortingEnabled = tableWidget->isSortingEnabled();
    tableWidget->setSortingEnabled( false );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnName,     item_Name );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnRegName,  item_RegName );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnType,     item_Type );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnProtocol, item_Protocol );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnBindHost, item_BindHost );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnBindPort, item_BindPort );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnHosts,    item_AgentHost );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnStatus,   item_Status );
    tableWidget->setSortingEnabled( isSortingEnabled );

    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnName,     QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnRegName,  QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnType,     QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnProtocol, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnBindHost, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnBindPort, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnStatus,   QHeaderView::ResizeToContents );

    tableWidget->verticalHeader()->setSectionResizeMode(tableWidget->rowCount() - 1, QHeaderView::ResizeToContents);

    adaptixWidget->Listeners.push_back(newListener);
}

void ListenersWidget::EditListenerItem(const ListenerData &newListener) const
{
    for ( int i = 0; i < adaptixWidget->Listeners.size(); i++ ) {
        if( adaptixWidget->Listeners[i].Name == newListener.Name ) {
            adaptixWidget->Listeners[i].BindHost = newListener.BindHost;
            adaptixWidget->Listeners[i].BindPort = newListener.BindPort;
            adaptixWidget->Listeners[i].AgentAddresses = newListener.AgentAddresses;
            adaptixWidget->Listeners[i].Status = newListener.Status;
            adaptixWidget->Listeners[i].Data = newListener.Data;
            break;
        }
    }

    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, ColumnName);
        if ( item && item->text() == newListener.Name ) {
            tableWidget->item(row, ColumnBindHost)->setText(newListener.BindHost);
            tableWidget->item(row, ColumnBindPort)->setText(newListener.BindPort);
            tableWidget->item(row, ColumnHosts)->setText(newListener.AgentAddresses);
            tableWidget->item(row, ColumnStatus)->setText(newListener.Status);

            if ( newListener.Status == "Listen"  )
                tableWidget->item(row, ColumnStatus)->setForeground(QColor(COLOR_NeonGreen) );
            else
                tableWidget->item(row, ColumnStatus)->setForeground( QColor(COLOR_ChiliPepper) );
            break;
        }
    }
}

void ListenersWidget::RemoveListenerItem(const QString &listenerName) const
{
    for ( int i = 0; i < adaptixWidget->Listeners.size(); i++ ) {
        if( adaptixWidget->Listeners[i].Name == listenerName ) {
            adaptixWidget->Listeners.erase( adaptixWidget->Listeners.begin() + i );
            break;
        }
    }

    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, ColumnName);
        if ( item && item->text() == listenerName ) {
            tableWidget->removeRow(row);
            break;
        }
    }
}

/// Slots

void ListenersWidget::handleListenersMenu(const QPoint &pos ) const
{
    QMenu listenerMenu = QMenu();

    listenerMenu.addAction("Create", this, &ListenersWidget::onCreateListener );
    listenerMenu.addAction("Edit",   this, &ListenersWidget::onEditListener );
    listenerMenu.addAction("Remove", this, &ListenersWidget::onRemoveListener );
    listenerMenu.addSeparator();
    listenerMenu.addAction("Generate agent", this, &ListenersWidget::onGenerateAgent );

    QPoint globalPos = tableWidget->mapToGlobal(pos);
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
    if (tableWidget->selectionModel()->selectedRows().empty())
        return;

    auto listenerName    = tableWidget->item( tableWidget->currentRow(), ColumnName )->text();
    auto listenerRegName = tableWidget->item( tableWidget->currentRow(), ColumnRegName )->text();

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
    if (tableWidget->selectionModel()->selectedRows().empty())
        return;

    auto listenerName    = tableWidget->item( tableWidget->currentRow(), ColumnName )->text();
    auto listenerRegName = tableWidget->item( tableWidget->currentRow(), ColumnRegName )->text();

    QMessageBox::StandardButton reply = QMessageBox::question(nullptr, "Delete Confirmation",
                                      QString("Are you sure you want to remove the '%1' listener?").arg(listenerName),
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    QString message = QString();
    bool ok = false;
    bool result = HttpReqListenerStop( listenerName, listenerRegName, *(adaptixWidget->GetProfile()), &message, &ok );
    if( !result ){
        MessageError("Response timeout");
        return;
    }

    if ( !ok ) {
        MessageError(message);
    }
}

void ListenersWidget::onGenerateAgent() const
{
    if (tableWidget->selectionModel()->selectedRows().empty())
        return;

    auto listenerName     = tableWidget->item( tableWidget->currentRow(), ColumnName )->text();
    auto listenerRegName  = tableWidget->item( tableWidget->currentRow(), ColumnRegName )->text();

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

        QJSValueList args;
        args << QJSValue(listenerRegName);
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

    DialogAgent* dialogListener = new DialogAgent(listenerName, listenerRegName);
    dialogListener->setAttribute(Qt::WA_DeleteOnClose);
    dialogListener->SetProfile( *(adaptixWidget->GetProfile()) );
    dialogListener->AddExAgents(agents, ax_uis);
    dialogListener->Start();
}
