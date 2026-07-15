#include <Agent/Agent.h>
#include <UI/Widgets/ListenersFeedWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>
#include <Client/Requestor.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <UI/Dialogs/DialogListener.h>
#include <UI/Dialogs/DialogAgent.h>
#include <Utils/FontManager.h>
#include <MainAdaptix.h>

#include <oclero/qlementine/widgets/Menu.hpp>

#include <QPainter>
#include <QJSEngine>
#include <QInputDialog>

namespace ListenersBlock {
    enum {
        Id = 0,
        Main = 1,
        Tags = 2,
        Right = 3,
        Count = 4
    };
}

static FeedRow listenerToFeedRow(const ListenerData& l) {
    FeedRow row;
    row.resize(ListenersBlock::Count);
    row.entityId = 0;
    row[ListenersBlock::Id] = QVariantMap{{"id", l.Name}, {"badge", l.ListenerProtocol}, {"date", QDateTime::fromSecsSinceEpoch(l.DateTimestamp).toString("dd/MM HH:mm:ss")}};

    QString bindText;
    if (!l.BindHost.isEmpty() && !l.BindPort.isEmpty())
        bindText = l.BindHost + " : " + l.BindPort;
    else if (!l.BindHost.isEmpty())
        bindText = l.BindHost;
    else if (!l.BindPort.isEmpty())
        bindText = ":" + l.BindPort;
    else
        bindText = "-";

    row[ListenersBlock::Main] = QVariantMap{{"main", bindText}, {"submain", l.ListenerRegName}, {"second", l.ListenerType + ": " + l.AgentAddresses}};
    row[ListenersBlock::Tags] = l.Tags.isEmpty() ? QStringList{} : l.Tags.split(",", Qt::SkipEmptyParts);
    row[ListenersBlock::Right] = QVariantMap{{"main", QString()}, {"second", QString()}, {"status", l.Status}, {"statusType", l.Status == "Listen" ? "success" : "error"}, {"dateNum", l.DateTimestamp}};
    row.isDead = false;
    return row;
}

static ListFeedDelegate* createListenersDelegate(QObject* parent) {
    auto* d = new ListFeedDelegate(parent);
    d->addBlock(new IdBadgeBlock());
    d->addBlock(new MainBlock());
    d->addBlock(new TagsBlock());
    d->addBlock(new StatusBlock());
    d->addBlock(new GroupHeaderBlock());
    return d;
}



ListenersFilterProxy::ListenersFilterProxy(QObject* parent) : QSortFilterProxyModel(parent) {}

void ListenersFilterProxy::setSearchText(const QString& text) { m_searchText = text; }
void ListenersFilterProxy::setProtocol(const QString& protocol) { m_protocol = protocol; }

bool ListenersFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    auto* srcModel = qobject_cast<FeedListModel*>(sourceModel());
    if (!srcModel || sourceRow < 0 || sourceRow >= srcModel->size())
        return true;

    const FeedRow& row = srcModel->rowAt(sourceRow);

    // Protocol filter
    if (!m_protocol.isEmpty()) {
        QString proto = row.blockData[ListenersBlock::Id].toMap()["badge"].toString();
        if (proto != m_protocol)
            return false;
    }

    // Search filter
    if (!m_searchText.isEmpty()) {
        QString lower = m_searchText.toLower();
        for (int i = 0; i < row.size(); ++i) {
            QString text = row.blockData[i].toString().toLower();
            if (text.contains(lower))
                return true;
            auto map = row.blockData[i].toMap();
            for (auto it = map.begin(); it != map.end(); ++it) {
                if (it.value().toString().toLower().contains(lower))
                    return true;
            }
            auto list = row.blockData[i].toStringList();
            for (const auto& s : list) {
                if (s.toLower().contains(lower))
                    return true;
            }
        }
        return false;
    }

    return true;
}



ListenersFeedWidget::ListenersFeedWidget(AdaptixWidget* w) : ListFeedWidget(w), m_adaptixWidget(w)
{
    feedBlockModel = new FeedListModel(this);
    auto* delegate = createListenersDelegate(this);
    delegate->setFeedModel(feedBlockModel);

    filterProxy = new ListenersFilterProxy(this);
    filterProxy->setSourceModel(feedBlockModel);

    setModel(feedBlockModel);
    setDelegate(delegate);
    setFilterModel(filterProxy);
    rebuildModelChain();

    enableSearch(true);
    enableFilterCombo(true, "All protocols");
    enableSortingCombo(true, {"No sorting", "Date", "Status", "Protocol", "Type", "Name", "RegName"});
    enableGroupCombo(true);
    finalizeSearchWidget();

    enableCompactSwitch(true);
    setBlockGap(12);

    auto* addBtn = new QPushButton("+ Add Listener", this);
    connect(addBtn, &QPushButton::clicked, this, &ListenersFeedWidget::onCreateListener);
    addToolbarWidgetAfter(addBtn);

    if (groupCombo()) {
        groupCombo()->clear();
        groupCombo()->addItem("No grouping");
        groupCombo()->addItem("By Protocol");
        groupCombo()->addItem("By Type");
        groupCombo()->addItem("By Status");
    }

    dockWidget = new KDDockWidgets::QtWidgets::DockWidget("ListenersFeed:Dock-" + w->GetProfile()->GetProject(), KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    dockWidget->setTitle("Listeners");
    dockWidget->setWidget(this);
    dockWidget->setIcon(QIcon(":/icons/listeners"), KDDockWidgets::IconPlace::TabBar);

    connect(treeView(), &QTreeView::customContextMenuRequested, this, &ListenersFeedWidget::handleFeedMenu);
}

ListenersFeedWidget::~ListenersFeedWidget() = default;

KDDockWidgets::QtWidgets::DockWidget* ListenersFeedWidget::dock() { return dockWidget; }

void ListenersFeedWidget::SetUpdatesEnabled(bool enabled)
{
    treeView()->setUpdatesEnabled(enabled);
}

void ListenersFeedWidget::onFilterChanged()
{
    if (!filterProxy)
        return;

    QString searchText = searchInput() ? searchInput()->text() : QString();
    QString protocol = filterCombo() && filterCombo()->currentIndex() > 0 ? filterCombo()->currentText() : QString();

    filterProxy->setSearchText(searchText);
    filterProxy->setProtocol(protocol);
    filterProxy->invalidate();
}

void ListenersFeedWidget::onGroupModeChanged(int index)
{
    if (!groupingProxy())
        return;

    if (index == 0) {
        groupingProxy()->setViewMode(VM_Flat);
    } else {
        int blockIdx = -1;
        QString fieldKey;
        switch (index) {
            case 1: blockIdx = ListenersBlock::Id; fieldKey = "badge"; break;     // By Protocol
            case 2: blockIdx = ListenersBlock::Main; fieldKey = "second"; break;  // By Type
            case 3: blockIdx = ListenersBlock::Right; fieldKey = "status"; break; // By Status
        }
        if (blockIdx >= 0) {
            feedBlockModel->setGroupKeySource(blockIdx, fieldKey);
            groupingProxy()->setGroupKeyRole(FeedListModel::GroupKeyRole);
            groupingProxy()->setAutoGroupField(AG_ByRole);
            groupingProxy()->setViewMode(VM_AutoGroup);
        }
    }
    treeView()->setRootIsDecorated(index > 0);
    treeView()->expandAll();
}

void ListenersFeedWidget::onSortingChanged(int index)
{
    if (!feedBlockModel || index == 0)
        return;

    Qt::SortOrder order = isSortAscending() ? Qt::AscendingOrder : Qt::DescendingOrder;

    switch (index) {
        case 1: feedBlockModel->sortByFieldNumeric(ListenersBlock::Right, "dateNum", order); break; // Date
        case 2: feedBlockModel->sortByField(ListenersBlock::Right, "status", order); break;         // Status
        case 3: feedBlockModel->sortByField(ListenersBlock::Id, "badge", order); break;             // Protocol
        case 4: feedBlockModel->sortByField(ListenersBlock::Main, "second", order); break;          // Type
        case 5: feedBlockModel->sortByField(ListenersBlock::Main, "main", order); break;            // Name
        case 6: feedBlockModel->sortByField(ListenersBlock::Main, "submain", order); break;         // RegName
    }
}

void ListenersFeedWidget::Clear()
{
    if (feedBlockModel) feedBlockModel->clear();
}

void ListenersFeedWidget::AddListenerItem(const ListenerData& newListener)
{
    if (!feedBlockModel)
        return;

    for (int i = 0; i < feedBlockModel->size(); ++i) {
        if (feedBlockModel->rowAt(i).blockData[ListenersBlock::Id].toMap()["id"].toString() == newListener.Name) {
            feedBlockModel->updateRow(i, listenerToFeedRow(newListener));
            for (int j = 0; j < m_adaptixWidget->Listeners.size(); ++j) {
                if (m_adaptixWidget->Listeners[j].Name == newListener.Name) {
                    m_adaptixWidget->Listeners[j] = newListener;
                    break;
                }
            }
            return;
        }
    }

    feedBlockModel->insertRow(0, listenerToFeedRow(newListener));
    m_adaptixWidget->Listeners.append(newListener);

    if (filterCombo()) {
        QString proto = newListener.ListenerProtocol;
        bool found = false;
        for (int i = 0; i < filterCombo()->count(); ++i) {
            if (filterCombo()->itemText(i) == proto) {
                found = true;
                break;
            }
        }
        if (!found && !proto.isEmpty())
            filterCombo()->addItem(proto);
    }
}

void ListenersFeedWidget::EditListenerItem(const ListenerData& newListener)
{
    if (!feedBlockModel)
        return;

    for (int i = 0; i < feedBlockModel->size(); ++i) {
        const FeedRow& r = feedBlockModel->rowAt(i);
        if (r.blockData[ListenersBlock::Id].toMap()["id"].toString() == newListener.Name) {
            FeedRow newRow = listenerToFeedRow(newListener);
            feedBlockModel->updateRow(i, newRow);
            break;
        }
    }
    for (int i = 0; i < m_adaptixWidget->Listeners.size(); ++i) {
        if (m_adaptixWidget->Listeners[i].Name == newListener.Name) {
            m_adaptixWidget->Listeners[i] = newListener;
            break;
        }
    }
}

void ListenersFeedWidget::RemoveListenerItem(const QString& listenerName)
{
    if (!feedBlockModel)
        return;

    for (int i = 0; i < feedBlockModel->size(); ++i) {
        const FeedRow& r = feedBlockModel->rowAt(i);
        if (r.blockData[ListenersBlock::Id].toMap()["id"].toString() == listenerName) {
            feedBlockModel->removeRow(i);
            break;
        }
    }
    for (int i = 0; i < m_adaptixWidget->Listeners.size(); ++i) {
        if (m_adaptixWidget->Listeners[i].Name == listenerName) {
            m_adaptixWidget->Listeners.removeAt(i);
            break;
        }
    }
}

void ListenersFeedWidget::handleFeedMenu(const QPoint& pos)
{
    oclero::qlementine::Menu ctxMenu;

    QModelIndex index = prepareContextMenuSelection(pos);
    if (index.isValid()) {
        QModelIndex srcIdx = index;
        QAbstractItemModel* m = const_cast<QAbstractItemModel*>(index.model());
        while (m && m != feedBlockModel) {
            auto* sp = qobject_cast<QSortFilterProxyModel*>(m);
            if (sp) {
                srcIdx = sp->mapToSource(srcIdx);
                m = sp->sourceModel();
                continue;
            }
            auto* gp = qobject_cast<GroupingProxyModel*>(m);
            if (gp) {
                srcIdx = gp->mapToSource(srcIdx);
                m = gp->sourceModel();
                continue;
            }
            break;
        }
        if (srcIdx.isValid() && srcIdx.row() >= 0 && srcIdx.row() < feedBlockModel->size()) {
            const FeedRow& row = feedBlockModel->rowAt(srcIdx.row());
            QString listenerName = row.blockData[ListenersBlock::Id].toMap()["id"].toString();
            if (!listenerName.isEmpty()) {
                ctxMenu.addSeparator();
                ctxMenu.addAction("Edit", this, &ListenersFeedWidget::onEditListener);
                ctxMenu.addAction("Remove", this, &ListenersFeedWidget::onRemoveListener);
                ctxMenu.addSeparator();
                ctxMenu.addAction("Pause", this, &ListenersFeedWidget::onPauseListener);
                ctxMenu.addAction("Resume", this, &ListenersFeedWidget::onResumeListener);
                ctxMenu.addSeparator();
                ctxMenu.addAction("Set tag", this, &ListenersFeedWidget::onSetTag);
                ctxMenu.addSeparator();
                ctxMenu.addAction("Generate Agent", this, &ListenersFeedWidget::onGenerateAgent);
                // ctxMenu.addAction("Create Connector", this, &ListenersFeedWidget::onCreateConnector);
            }
        }
    }

    ctxMenu.exec(treeView()->viewport()->mapToGlobal(pos));
}

void ListenersFeedWidget::onCreateListener()
{
    if (!m_adaptixWidget)
        return;

    QList<RegListenerConfig> listeners;
    QMap<QString, AxUI> ax_uis;

    auto listenersList = m_adaptixWidget->ScriptManager->ListenerScriptList();
    for (const auto& listener : listenersList) {
        auto* engine = m_adaptixWidget->ScriptManager->ListenerScriptEngine(listener);
        if (!engine) {
            m_adaptixWidget->ScriptManager->consolePrintError(QString("Listener %1 is not registered").arg(listener));
            continue;
        }

        QJSValue func = engine->globalObject().property("ListenerUI");
        if (!func.isCallable()) {
            m_adaptixWidget->ScriptManager->consolePrintError(listener + " - function ListenerUI is not registered");
            continue;
        }

        QJSValueList args;
        args << QJSValue(true);
        QJSValue result = func.call(args);
        if (result.isError()) {
            m_adaptixWidget->ScriptManager->consolePrintError(result.toString());
            continue;
        }
        if (!result.isObject())
            continue;

        QJSValue ui_container = result.property("ui_container");
        QJSValue ui_panel     = result.property("ui_panel");
        QJSValue ui_height    = result.property("ui_height");
        QJSValue ui_width     = result.property("ui_width");

        if (ui_container.isUndefined() || !ui_container.isObject() || ui_panel.isUndefined() || !ui_panel.isQObject())
            continue;

        auto* formElement = dynamic_cast<AxPanelWrapper*>(ui_panel.toQObject());
        auto* container = dynamic_cast<AxContainerWrapper*>(ui_container.toQObject());
        if (!formElement || !container)
            continue;

        int h = ui_height.isNumber() && ui_height.toInt() > 0 ? ui_height.toInt() : 650;
        int w = ui_width.isNumber() && ui_width.toInt() > 0 ? ui_width.toInt() : 650;

        auto regListener = m_adaptixWidget->GetRegListener(listener);
        listeners.append(regListener);
        ax_uis[listener] = { container, formElement->widget(), h, w };
    }

    auto* dialogListener = new DialogListener();
    dialogListener->setAttribute(Qt::WA_DeleteOnClose);
    dialogListener->SetProfile(*(m_adaptixWidget->GetProfile()));
    dialogListener->AddExListeners(listeners, ax_uis);
    dialogListener->Start();
}

ListenersFeedWidget::ListenerInfo ListenersFeedWidget::currentListenerInfo() const
{
    QModelIndex idx = treeView()->currentIndex();
    if (!idx.isValid() || !feedBlockModel)
        return {};

    QModelIndex srcIdx = idx;
    QAbstractItemModel* m = const_cast<QAbstractItemModel*>(idx.model());
    while (m && m != feedBlockModel) {
        auto* sp = qobject_cast<QSortFilterProxyModel*>(m);
        if (sp) {
            srcIdx = sp->mapToSource(srcIdx);
            m = sp->sourceModel();
            continue;
        }
        auto* gp = qobject_cast<GroupingProxyModel*>(m);
        if (gp) {
            srcIdx = gp->mapToSource(srcIdx);
            m = gp->sourceModel();
            continue;
        }
        break;
    }
    if (!srcIdx.isValid() || srcIdx.row() < 0 || srcIdx.row() >= feedBlockModel->size())
        return {};

    const FeedRow& r = feedBlockModel->rowAt(srcIdx.row());
    auto idMap = r.blockData[ListenersBlock::Id].toMap();
    auto mainMap = r.blockData[ListenersBlock::Main].toMap();
    ListenerInfo info;
    info.name = idMap["id"].toString();
    info.regName = mainMap["submain"].toString();
    info.tags = r.blockData[ListenersBlock::Tags].toStringList().join(",");
    info.valid = !info.name.isEmpty();
    return info;
}

void ListenersFeedWidget::onEditListener()
{
    ListenerInfo info = currentListenerInfo();
    if (!info.valid)
        return;

    QString listenerData;
    for (const auto& l : m_adaptixWidget->Listeners) {
        if (l.Name == info.name) {
            listenerData = l.Data;
            break;
        }
    }

    auto* engine = m_adaptixWidget->ScriptManager->ListenerScriptEngine(info.regName);
    if (!engine) {
        m_adaptixWidget->ScriptManager->consolePrintError(QString("Listener %1 is not registered").arg(info.name));
        return;
    }

    QJSValue func = engine->globalObject().property("ListenerUI");
    if (!func.isCallable()) {
        m_adaptixWidget->ScriptManager->consolePrintError(info.name + " - function ListenerUI is not registered");
        return;
    }

    QJSValueList args;
    args << QJSValue(false);
    QJSValue result = func.call(args);
    if (result.isError()) {
        m_adaptixWidget->ScriptManager->consolePrintError(result.toString());
        return;
    }
    if (!result.isObject())
        return;

    QJSValue ui_container = result.property("ui_container");
    QJSValue ui_panel     = result.property("ui_panel");
    QJSValue ui_height    = result.property("ui_height");
    QJSValue ui_width     = result.property("ui_width");

    if (ui_container.isUndefined() || !ui_container.isObject() || ui_panel.isUndefined() || !ui_panel.isQObject())
        return;

    auto* formElement = dynamic_cast<AxPanelWrapper*>(ui_panel.toQObject());
    auto* container = dynamic_cast<AxContainerWrapper*>(ui_container.toQObject());
    if (!formElement || !container)
        return;

    int h = ui_height.isNumber() && ui_height.toInt() > 0 ? ui_height.toInt() : 650;
    int w = ui_width.isNumber() && ui_width.toInt() > 0 ? ui_width.toInt() : 650;

    QList<RegListenerConfig> listeners;
    QMap<QString, AxUI> ax_uis;
    auto regListener = m_adaptixWidget->GetRegListener(info.regName);
    listeners.append(regListener);
    ax_uis[info.regName] = { container, formElement->widget(), h, w };

    container->fromJson(listenerData);

    auto* dialogListener = new DialogListener();
    dialogListener->setAttribute(Qt::WA_DeleteOnClose);
    dialogListener->SetProfile(*(m_adaptixWidget->GetProfile()));
    dialogListener->AddExListeners(listeners, ax_uis);
    dialogListener->SetEditMode(info.name);
    dialogListener->Start();
}

void ListenersFeedWidget::onRemoveListener()
{
    ListenerInfo info = currentListenerInfo();
    if (!info.valid)
        return;

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Remove Listener", "Remove listener \"" + info.name + "\"?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    HttpReqListenerStopAsync(info.name, info.regName, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success) MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void ListenersFeedWidget::onPauseListener()
{
    ListenerInfo info = currentListenerInfo();
    if (!info.valid)
        return;

    HttpReqListenerPauseAsync(info.name, info.regName, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success) MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void ListenersFeedWidget::onResumeListener()
{
    ListenerInfo info = currentListenerInfo();
    if (!info.valid)
        return;

    HttpReqListenerResumeAsync(info.name, info.regName, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success) MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void ListenersFeedWidget::onSetTag()
{
    ListenerInfo info = currentListenerInfo();
    if (!info.valid)
        return;

    bool inputOk;
    QString newTag = QInputDialog::getText(this, "Set tags", "New tag", QLineEdit::Normal, info.tags, &inputOk);
    if (inputOk) {
        HttpReqListenerSetTagsAsync(info.name, newTag, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success) MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void ListenersFeedWidget::onGenerateAgent()
{
    ListenerInfo info = currentListenerInfo();
    if (!info.valid)
        return;

    QList<QString> agentNames = m_adaptixWidget->GetAgentNames(info.regName);

    QStringList agents;
    QMap<QString, AxUI> ax_uis;

    for (const auto& agent : agentNames) {
        auto* engine = m_adaptixWidget->ScriptManager->AgentScriptEngine(agent);
        if (!engine) {
            m_adaptixWidget->ScriptManager->consolePrintError(QString("Agent %1 is not registered").arg(agent));
            return;
        }

        QJSValue func = engine->globalObject().property("GenerateUI");
        if (!func.isCallable()) {
            m_adaptixWidget->ScriptManager->consolePrintError(agent + " - function GenerateUI is not registered");
            return;
        }

        QJSValue jsListeners = engine->newArray(1);
        jsListeners.setProperty(0, info.regName);

        QJSValueList args;
        args << jsListeners;
        QJSValue result = func.call(args);
        if (result.isError()) {
            m_adaptixWidget->ScriptManager->consolePrintError(result.toString());
            return;
        }
        if (!result.isObject())
            return;

        QJSValue ui_container = result.property("ui_container");
        QJSValue ui_panel     = result.property("ui_panel");
        QJSValue ui_height    = result.property("ui_height");

        if (ui_container.isUndefined() || !ui_container.isObject() || ui_panel.isUndefined() || !ui_panel.isQObject())
            return;

        auto* formElement = dynamic_cast<AxPanelWrapper*>(ui_panel.toQObject());
        auto* container = dynamic_cast<AxContainerWrapper*>(ui_container.toQObject());
        if (!formElement || !container)
            return;

        int h = ui_height.isNumber() && ui_height.toInt() > 0 ? ui_height.toInt() : 650;

        agents.append(agent);
        ax_uis[agent] = { container, formElement->widget(), h, 650 };
    }

    auto* dialogAgent = new DialogAgent(m_adaptixWidget, info.name, info.regName);
    dialogAgent->setAttribute(Qt::WA_DeleteOnClose);
    dialogAgent->SetProfile(*(m_adaptixWidget->GetProfile()));
    dialogAgent->SetAgentTypes(m_adaptixWidget->AgentTypes);
    dialogAgent->SetAvailableListeners(m_adaptixWidget->Listeners);
    dialogAgent->AddExAgents(agents, ax_uis);
    dialogAgent->Start();
}

void ListenersFeedWidget::onCreateConnector()
{
    ListenerInfo info = currentListenerInfo();
    if (!info.valid)
        return;

    auto* engine = m_adaptixWidget->ScriptManager->ListenerScriptEngine(info.regName);
    if (!engine) {
        m_adaptixWidget->ScriptManager->consolePrintError(QString("Listener %1 is not registered").arg(info.regName));
        return;
    }

    QJSValue func = engine->globalObject().property("ConnectorUI");
    if (!func.isCallable()) {
        m_adaptixWidget->ScriptManager->consolePrintError(info.regName + " - function ConnectorUI is not registered");
        return;
    }

    func.call({QJSValue(true)});
}
