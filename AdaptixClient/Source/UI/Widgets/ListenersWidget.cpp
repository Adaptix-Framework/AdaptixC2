#include <QJSEngine>
#include <UI/Widgets/ListenersWidget.h>
#include <UI/Dialogs/DialogListener.h>
#include <UI/Dialogs/DialogAgent.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/Requestor.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <Client/AxScript/AxScriptManager.h>

ListenersWidget::ListenersWidget(AdaptixWidget* w) : adaptixWidget(w)
{
    this->createUI();

    connect(tableWidget, &QTableWidget::customContextMenuRequested, this, &ListenersWidget::handleListenersMenu);
}

ListenersWidget::~ListenersWidget() = default;

void ListenersWidget::createUI()
{
    tableWidget = new QTableWidget( this );
    tableWidget->setColumnCount( 6 );
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

    tableWidget->setHorizontalHeaderItem( 0, new QTableWidgetItem( "Name" ) );
    tableWidget->setHorizontalHeaderItem( 1, new QTableWidgetItem( "Type" ) );
    tableWidget->setHorizontalHeaderItem( 2, new QTableWidgetItem( "Bind Host" ) );
    tableWidget->setHorizontalHeaderItem( 3, new QTableWidgetItem( "Bind Port" ) );
    tableWidget->setHorizontalHeaderItem( 4, new QTableWidgetItem( "C2 Hosts (agent)" ) );
    tableWidget->setHorizontalHeaderItem( 5, new QTableWidgetItem( "Status" ) );

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->setContentsMargins( 0, 0,  0, 0);
    mainGridLayout->addWidget( tableWidget, 0, 0, 1, 1);
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
        if( listener.ListenerName == newListener.ListenerName )
            return;
    }

    auto item_Name      = new QTableWidgetItem( newListener.ListenerName );
    auto item_Type      = new QTableWidgetItem( newListener.ListenerType );
    auto item_BindHost  = new QTableWidgetItem( newListener.BindHost );
    auto item_BindPort  = new QTableWidgetItem( newListener.BindPort );
    auto item_AgentHost = new QTableWidgetItem( newListener.AgentAddresses );
    auto item_Status    = new QTableWidgetItem( newListener.Status );

    item_Name->setFlags( item_Name->flags() ^ Qt::ItemIsEditable );

    item_Type->setFlags( item_Type->flags() ^ Qt::ItemIsEditable );

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
    tableWidget->setItem( tableWidget->rowCount() - 1, 0, item_Name );
    tableWidget->setItem( tableWidget->rowCount() - 1, 1, item_Type );
    tableWidget->setItem( tableWidget->rowCount() - 1, 2, item_BindHost );
    tableWidget->setItem( tableWidget->rowCount() - 1, 3, item_BindPort );
    tableWidget->setItem( tableWidget->rowCount() - 1, 4, item_AgentHost );
    tableWidget->setItem( tableWidget->rowCount() - 1, 5, item_Status );
    tableWidget->setSortingEnabled( isSortingEnabled );

    tableWidget->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 3, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 5, QHeaderView::ResizeToContents );

    tableWidget->verticalHeader()->setSectionResizeMode(tableWidget->rowCount() - 1, QHeaderView::ResizeToContents);

    adaptixWidget->Listeners.push_back(newListener);
}

void ListenersWidget::EditListenerItem(const ListenerData &newListener) const
{
    for ( int i = 0; i < adaptixWidget->Listeners.size(); i++ ) {
        if( adaptixWidget->Listeners[i].ListenerName == newListener.ListenerName ) {
            adaptixWidget->Listeners[i].BindHost = newListener.BindHost;
            adaptixWidget->Listeners[i].BindPort = newListener.BindPort;
            adaptixWidget->Listeners[i].AgentAddresses = newListener.AgentAddresses;
            adaptixWidget->Listeners[i].Status = newListener.Status;
            adaptixWidget->Listeners[i].Data = newListener.Data;
            break;
        }
    }

    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, 0);
        if ( item && item->text() == newListener.ListenerName ) {
            tableWidget->item(row, 2)->setText(newListener.BindHost);
            tableWidget->item(row, 3)->setText(newListener.BindPort);
            tableWidget->item(row, 4)->setText(newListener.AgentAddresses);
            tableWidget->item(row, 5)->setText(newListener.Status);

            if ( newListener.Status == "Listen"  )
                tableWidget->item(row, 5)->setForeground(QColor(COLOR_NeonGreen) );
            else
                tableWidget->item(row, 5)->setForeground( QColor(COLOR_ChiliPepper) );
            break;
        }
    }
}

void ListenersWidget::RemoveListenerItem(const QString &listenerName) const
{
    for ( int i = 0; i < adaptixWidget->Listeners.size(); i++ ) {
        if( adaptixWidget->Listeners[i].ListenerName == listenerName ) {
            adaptixWidget->Listeners.erase( adaptixWidget->Listeners.begin() + i );
            break;
        }
    }

    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, 0);
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
    QStringList listeners;
    QMap<QString, QWidget*> widgets;
    QMap<QString, AxContainerWrapper*> containers;

    auto listenersList = adaptixWidget->ScriptManager->ListenerScriptList();

    for (auto listener : listenersList) {
        auto engine = adaptixWidget->ScriptManager->ListenerScriptEngine(listener);
        if (engine == nullptr) {
            // emit QJSValue::TypeError, QString("Listener %1 is not registered").arg(name) /// ToDo error
            continue;
        }

        QJSValue func = engine->globalObject().property("ListenerUI");
        if (!func.isCallable()) {
            // emit QJSValue::TypeError, "Function ListenerUI is not registered")
            continue;
        }

        QJSValueList args;
        args << QJSValue(true);
        QJSValue result = func.call(args);
        if (result.isError()) {
            QString error = QStringLiteral("%1\n  at line %2 in %3\n  stack: %4").arg(result.toString()).arg(result.property("lineNumber").toInt()).arg(result.property("fileName").toString()).arg(result.property("stack").toString());
            // emit consoleAppendError(error);
            continue;
        }

        if (!result.isObject()) {
            // emit consoleAppendError("ListenerUI() must return panel and container objects");
            continue;
        }

        QJSValue ui_container = result.property("ui_container");
        QJSValue ui_panel = result.property("ui_panel");
        if ( ui_container.isUndefined() || !ui_container.isObject() || ui_panel.isUndefined() || !ui_panel.isQObject()) {
            // emit consoleAppendError("ListenerUI() must return panel and container objects");
            continue;
        }

        QObject* objPanel = ui_panel.toQObject();
        auto* formElement = dynamic_cast<AxPanelWrapper*>(objPanel);
        if (!formElement) {
            // emit consoleAppendError("ListenerUI() must return panel and container objects");
            continue;
        }

        QObject* objContainer = ui_container.toQObject();
        auto* container = dynamic_cast<AxContainerWrapper*>(objContainer);
        if (!container) {
            // emit consoleAppendError("ListenerUI() must return panel and container objects");
            continue;
        }

        listeners.append(listener);
        widgets[listener] = formElement->widget();
        containers[listener] = container;
    }

    DialogListener* dialogListener = new DialogListener();
    dialogListener->setAttribute(Qt::WA_DeleteOnClose);
    dialogListener->SetProfile( *(adaptixWidget->GetProfile()) );
    dialogListener->AddExListeners(listeners, widgets, containers);
    dialogListener->Start();
}

void ListenersWidget::onEditListener() const
{
    if (tableWidget->selectionModel()->selectedRows().empty())
        return;

    auto listenerName = tableWidget->item( tableWidget->currentRow(), 0 )->text();
    auto listenerType = tableWidget->item( tableWidget->currentRow(), 1 )->text();

    QString listenerData = "";
    for (auto listener : adaptixWidget->Listeners) {
        if(listener.ListenerName == listenerName) {
            listenerData = listener.Data;
            break;
        }
    }

    QStringList listeners;
    QMap<QString, QWidget*> widgets;
    QMap<QString, AxContainerWrapper*> containers;

    auto engine = adaptixWidget->ScriptManager->ListenerScriptEngine(listenerType);
    if (engine == nullptr) {
        // emit QJSValue::TypeError, QString("Listener %1 is not registered").arg(name) /// ToDo error
        return;;
    }

    QJSValue func = engine->globalObject().property("ListenerUI");
    if (!func.isCallable()) {
        // emit QJSValue::TypeError, "Function ListenerUI is not registered")
        return;
    }

    QJSValueList args;
    args << QJSValue(false);
    QJSValue result = func.call(args);
    if (result.isError()) {
        QString error = QStringLiteral("%1\n  at line %2 in %3\n  stack: %4").arg(result.toString()).arg(result.property("lineNumber").toInt()).arg(result.property("fileName").toString()).arg(result.property("stack").toString());
        // emit consoleAppendError(error);
        return;
    }

    if (!result.isObject()) {
        // emit consoleAppendError("ListenerUI() must return panel and container objects");
        return;
    }

    QJSValue ui_container = result.property("ui_container");
    QJSValue ui_panel = result.property("ui_panel");
    if ( ui_container.isUndefined() || !ui_container.isObject() || ui_panel.isUndefined() || !ui_panel.isQObject()) {
        // emit consoleAppendError("ListenerUI() must return panel and container objects");
        return;
    }

    QObject* objPanel = ui_panel.toQObject();
    auto* formElement = dynamic_cast<AxPanelWrapper*>(objPanel);
    if (!formElement) {
        // emit consoleAppendError("ListenerUI() must return panel and container objects");
        return;
    }

    QObject* objContainer = ui_container.toQObject();
    auto* container = dynamic_cast<AxContainerWrapper*>(objContainer);
    if (!container) {
        // emit consoleAppendError("ListenerUI() must return panel and container objects");
        return;
    }

    listeners.append(listenerType);
    widgets[listenerType] = formElement->widget();
    containers[listenerType] = container;

    container->fromJson(listenerData);

    DialogListener* dialogListener = new DialogListener();
    dialogListener->setAttribute(Qt::WA_DeleteOnClose);
    dialogListener->SetEditMode(listenerName);
    dialogListener->SetProfile( *(adaptixWidget->GetProfile()) );
    dialogListener->AddExListeners(listeners, widgets, containers);
    dialogListener->Start();
}

void ListenersWidget::onRemoveListener() const
{
    if (tableWidget->selectionModel()->selectedRows().empty())
        return;

    auto listenerName = tableWidget->item( tableWidget->currentRow(), 0 )->text();
    auto listenerType = tableWidget->item( tableWidget->currentRow(), 1 )->text();

    QString message = QString();
    bool ok = false;
    bool result = HttpReqListenerStop( listenerName, listenerType, *(adaptixWidget->GetProfile()), &message, &ok );
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

    auto listenerName = tableWidget->item( tableWidget->currentRow(), 0 )->text();
    auto listenerType = tableWidget->item( tableWidget->currentRow(), 1 )->text();

    QStringList parts = listenerType.split("/");
    if (parts.size() != 3) {
        return;
    }
    QString targetListener = parts[2];
    QList<QString> agentNames = adaptixWidget->GetAgentNames(targetListener);

    QStringList agents;
    QMap<QString, QWidget*> widgets;
    QMap<QString, AxContainerWrapper*> containers;

    for (auto agent : agentNames) {
        auto engine = adaptixWidget->ScriptManager->AgentScriptEngine(agent);
        if (engine == nullptr) {
            // emit QJSValue::TypeError, QString("Listener %1 is not registered").arg(name) /// ToDo error
            return;;
        }

        QJSValue func = engine->globalObject().property("GenerateUI");
        if (!func.isCallable()) {
            // emit QJSValue::TypeError, "Function GenerateUI is not registered")
            return;
        }

        QJSValueList args;
        args << QJSValue(targetListener);
        QJSValue result = func.call(args);
        if (result.isError()) {
            QString error = QStringLiteral("%1\n  at line %2 in %3\n  stack: %4").arg(result.toString()).arg(result.property("lineNumber").toInt()).arg(result.property("fileName").toString()).arg(result.property("stack").toString());
            // emit consoleAppendError(error);
            return;
        }

        if (!result.isObject()) {
            // emit consoleAppendError("GenerateUI() must return panel and container objects");
            return;
        }

        QJSValue ui_container = result.property("ui_container");
        QJSValue ui_panel = result.property("ui_panel");
        if ( ui_container.isUndefined() || !ui_container.isObject() || ui_panel.isUndefined() || !ui_panel.isQObject()) {
            // emit consoleAppendError("GenerateUI() must return panel and container objects");
            return;
        }

        QObject* objPanel = ui_panel.toQObject();
        auto* formElement = dynamic_cast<AxPanelWrapper*>(objPanel);
        if (!formElement) {
            // emit consoleAppendError("GenerateUI() must return panel and container objects");
            return;
        }

        QObject* objContainer = ui_container.toQObject();
        auto* container = dynamic_cast<AxContainerWrapper*>(objContainer);
        if (!container) {
            // emit consoleAppendError("GenerateUI() must return panel and container objects");
            return;
        }

        agents.append(agent);
        widgets[agent] = formElement->widget();
        containers[agent] = container;
    }

    DialogAgent* dialogListener = new DialogAgent(listenerName, listenerType);
    dialogListener->setAttribute(Qt::WA_DeleteOnClose);
    dialogListener->SetProfile( *(adaptixWidget->GetProfile()) );
    dialogListener->AddExAgents(agents, widgets, containers);
    dialogListener->Start();
}
