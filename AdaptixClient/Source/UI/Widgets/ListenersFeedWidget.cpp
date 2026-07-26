#include <Agent/Agent.h>
#include <UI/Widgets/ListenersFeedWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>
#include <Client/Requestor.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <UI/Dialogs/DialogListener.h>
#include <UI/Dialogs/DialogAgent.h>
#include <Utils/CustomElements/ControlCard.h>
#include <Utils/FontManager.h>
#include <MainAdaptix.h>

#include <oclero/qlementine/widgets/Menu.hpp>
#include <oclero/qlementine/widgets/LineEdit.hpp>

#include <QJSEngine>
#include <QInputDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QDateTime>

static bool listenerIsActive(const ListenerData& l)
{
    const QString s = l.Status.trimmed();
    return s.compare(QStringLiteral("Listen"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("Running"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("Active"), Qt::CaseInsensitive) == 0;
}

ListenersFeedWidget::ListenersFeedWidget(AdaptixWidget* w) : QWidget(w), m_adaptixWidget(w)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* toolbar = new QWidget(this);
    auto* tb = new QHBoxLayout(toolbar);
    tb->setContentsMargins(8, 6, 8, 6);
    tb->setSpacing(8);

    auto* searchEdit = new oclero::qlementine::LineEdit(toolbar);
    searchEdit->setIcon(QIcon(QStringLiteral(":/icons/search")));
    searchEdit->setPlaceholderText(QStringLiteral("Search listeners..."));
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setMinimumWidth(160);
    searchEdit->setFixedHeight(FontManager::instance().typography().controlHeight);
    m_search = searchEdit;
    tb->addWidget(m_search, 1);

    m_protocolFilter = new QComboBox(toolbar);
    m_protocolFilter->addItem(QStringLiteral("All protocols"));
    m_protocolFilter->setMinimumWidth(120);
    tb->addWidget(m_protocolFilter, 0);

    m_addBtn = new QPushButton(QStringLiteral("+ Add Listener"), toolbar);
    tb->addWidget(m_addBtn, 0);

    m_cardList = new ControlCardList(this);

    root->addWidget(toolbar, 0);
    root->addWidget(m_cardList, 1);

    connect(m_search, &QLineEdit::textChanged, this, &ListenersFeedWidget::onSearchChanged);
    connect(m_protocolFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ListenersFeedWidget::onProtocolFilterChanged);
    connect(m_addBtn, &QPushButton::clicked, this, &ListenersFeedWidget::onCreateListener);

    connect(m_cardList, &ControlCardList::primaryActionClicked, this, &ListenersFeedWidget::onCardPrimary);
    connect(m_cardList, &ControlCardList::deleteClicked, this, &ListenersFeedWidget::onCardDelete);
    connect(m_cardList, &ControlCardList::generateClicked, this, &ListenersFeedWidget::onCardGenerate);
    connect(m_cardList, &ControlCardList::doubleClicked, this, &ListenersFeedWidget::onCardDoubleClick);
    connect(m_cardList, &ControlCardList::selectionChanged, this, &ListenersFeedWidget::onCardSelected);
    connect(m_cardList, &ControlCardList::contextMenuRequested, this, &ListenersFeedWidget::onCardContextMenu);

    dockWidget = new KDDockWidgets::QtWidgets::DockWidget("ListenersFeed:Dock-" + w->GetProfile()->GetProject(), KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    dockWidget->setTitle("Listeners");
    dockWidget->setWidget(this);
    dockWidget->setIcon(QIcon(":/icons/listeners"), KDDockWidgets::IconPlace::TabBar);
}

ListenersFeedWidget::~ListenersFeedWidget() = default;

KDDockWidgets::QtWidgets::DockWidget* ListenersFeedWidget::dock() { return dockWidget; }

void ListenersFeedWidget::SetUpdatesEnabled(bool enabled)
{
    setUpdatesEnabled(enabled);
    if (m_cardList)
        m_cardList->setUpdatesEnabled(enabled);
}

void ListenersFeedWidget::Clear()
{
    m_items.clear();
    m_selectedName.clear();
    if (m_cardList)
        m_cardList->clear();
    if (m_protocolFilter) {
        m_protocolFilter->blockSignals(true);
        while (m_protocolFilter->count() > 1)
            m_protocolFilter->removeItem(1);
        m_protocolFilter->blockSignals(false);
    }
}

ControlCardData ListenersFeedWidget::toCard(const ListenerData& l) const
{
    ControlCardData d;
    d.id = l.Name;
    d.bodyLayout = ControlCard::BodyThreeLine;
    d.contentStyle = ControlCard::StyleListener;

    d.title = l.Name.isEmpty() ? QStringLiteral("listener") : l.Name;

    QString typeVal;
    if (!l.ListenerProtocol.isEmpty())
        typeVal = l.ListenerProtocol.trimmed().toUpper();
    if (!l.ListenerType.isEmpty()) {
        const QString lt = l.ListenerType.trimmed();
        if (typeVal.isEmpty())
            typeVal = lt;
        else if (lt.compare(typeVal, Qt::CaseInsensitive) != 0)
            typeVal += QStringLiteral(" · ") + lt;
    }
    if (typeVal.isEmpty())
        typeVal = QStringLiteral("—");
    d.primaryPrefix = QStringLiteral("type");
    d.primary = typeVal;

    d.detailPrefix = QStringLiteral("bind");
    if (!l.BindHost.isEmpty() && !l.BindPort.isEmpty())
        d.detail = l.BindHost + QStringLiteral(" :") + l.BindPort;
    else if (!l.BindHost.isEmpty())
        d.detail = l.BindHost;
    else if (!l.BindPort.isEmpty())
        d.detail = QStringLiteral(":") + l.BindPort;
    else
        d.detail = QStringLiteral("—");

    if (!l.Tags.isEmpty())
        d.sideText = l.Tags;

    if (!l.Date.isEmpty())
        d.dateText = l.Date;
    else if (l.DateTimestamp > 0)
        d.dateText = QDateTime::fromSecsSinceEpoch(l.DateTimestamp).toString(QStringLiteral("dd/MM/yy HH:mm"));

    d.secondaryLead = l.ListenerRegName.trimmed().isEmpty() ? QStringLiteral("—") : l.ListenerRegName.trimmed();
    d.secondaryPrefix = QStringLiteral("CALLBACK");
    d.secondary = l.AgentAddresses.trimmed().isEmpty() ? QStringLiteral("—") : l.AgentAddresses.trimmed();

    d.status = l.Status.isEmpty() ? QStringLiteral("Unknown") : l.Status;
    d.active = listenerIsActive(l);
    d.showPrimaryAction = true;
    if (d.active) {
        d.primaryAction = ControlCard::ActionStop;
        d.primaryActionLabel = QStringLiteral("Pause");
    } else {
        d.primaryAction = ControlCard::ActionStart;
        d.primaryActionLabel = QStringLiteral("Resume");
    }
    d.showDelete = true;
    d.deleteActionLabel = QStringLiteral("Remove");
    d.showGenerate = true;
    d.generateActionLabel = QStringLiteral("Agent");
    return d;
}

bool ListenersFeedWidget::matchesFilter(const ListenerData& l) const
{
    if (m_protocolFilter && m_protocolFilter->currentIndex() > 0) {
        if (l.ListenerProtocol != m_protocolFilter->currentText())
            return false;
    }
    if (m_search) {
        const QString q = m_search->text().trimmed().toLower();
        if (!q.isEmpty()) {
            const QString hay = (l.Name + l.ListenerProtocol + l.ListenerRegName + l.ListenerType + l.BindHost + l.BindPort + l.AgentAddresses + l.Tags + l.Status).toLower();
            if (!hay.contains(q))
                return false;
        }
    }
    return true;
}

void ListenersFeedWidget::rebuildVisible()
{
    if (!m_cardList)
        return;
    QVector<ControlCardData> cards;
    cards.reserve(m_items.size());
    for (const auto& l : m_items) {
        if (matchesFilter(l))
            cards.append(toCard(l));
    }
    m_cardList->setCards(cards);
    if (!m_selectedName.isEmpty())
        m_cardList->setSelectedId(m_selectedName);
}

ListenerData* ListenersFeedWidget::findByName(const QString& name)
{
    for (auto& l : m_items) {
        if (l.Name == name)
            return &l;
    }
    return nullptr;
}

const ListenerData* ListenersFeedWidget::findByName(const QString& name) const
{
    for (const auto& l : m_items) {
        if (l.Name == name)
            return &l;
    }
    return nullptr;
}

void ListenersFeedWidget::AddListenerItem(const ListenerData& newListener)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].Name == newListener.Name) {
            m_items[i] = newListener;
            for (int j = 0; j < m_adaptixWidget->Listeners.size(); ++j) {
                if (m_adaptixWidget->Listeners[j].Name == newListener.Name) {
                    m_adaptixWidget->Listeners[j] = newListener;
                    break;
                }
            }
            if (m_cardList && matchesFilter(newListener))
                m_cardList->upsertCard(toCard(newListener));
            else
                rebuildVisible();
            return;
        }
    }

    m_items.prepend(newListener);
    m_adaptixWidget->Listeners.append(newListener);

    if (m_protocolFilter) {
        const QString proto = newListener.ListenerProtocol;
        bool found = false;
        for (int i = 0; i < m_protocolFilter->count(); ++i) {
            if (m_protocolFilter->itemText(i) == proto) {
                found = true;
                break;
            }
        }
        if (!found && !proto.isEmpty())
            m_protocolFilter->addItem(proto);
    }

    if (matchesFilter(newListener))
        m_cardList->upsertCard(toCard(newListener));
}

void ListenersFeedWidget::EditListenerItem(const ListenerData& newListener)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].Name == newListener.Name) {
            m_items[i] = newListener;
            break;
        }
    }
    for (int i = 0; i < m_adaptixWidget->Listeners.size(); ++i) {
        if (m_adaptixWidget->Listeners[i].Name == newListener.Name) {
            m_adaptixWidget->Listeners[i] = newListener;
            break;
        }
    }
    if (m_cardList) {
        if (matchesFilter(newListener))
            m_cardList->upsertCard(toCard(newListener));
        else
            m_cardList->removeCard(newListener.Name);
    }
}

void ListenersFeedWidget::RemoveListenerItem(const QString& listenerName)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].Name == listenerName) {
            m_items.removeAt(i);
            break;
        }
    }
    for (int i = 0; i < m_adaptixWidget->Listeners.size(); ++i) {
        if (m_adaptixWidget->Listeners[i].Name == listenerName) {
            m_adaptixWidget->Listeners.removeAt(i);
            break;
        }
    }
    if (m_selectedName == listenerName)
        m_selectedName.clear();
    if (m_cardList)
        m_cardList->removeCard(listenerName);
}

ListenersFeedWidget::ListenerInfo ListenersFeedWidget::currentListenerInfo() const
{
    ListenerInfo info;
    if (m_selectedName.isEmpty())
        return info;
    const ListenerData* l = findByName(m_selectedName);
    if (!l)
        return info;
    info.name = l->Name;
    info.regName = l->ListenerRegName;
    info.tags = l->Tags;
    info.valid = !info.name.isEmpty();
    return info;
}

void ListenersFeedWidget::onSearchChanged(const QString&)
{
    rebuildVisible();
}

void ListenersFeedWidget::onProtocolFilterChanged(int)
{
    rebuildVisible();
}

void ListenersFeedWidget::onCardSelected(const QVariant& id)
{
    m_selectedName = id.toString();
}

void ListenersFeedWidget::onCardPrimary(const QVariant& id)
{
    m_selectedName = id.toString();
    const ListenerData* l = findByName(m_selectedName);
    if (!l)
        return;
    if (listenerIsActive(*l))
        onPauseListener();
    else
        onResumeListener();
}

void ListenersFeedWidget::onCardDelete(const QVariant& id)
{
    m_selectedName = id.toString();
    onRemoveListener();
}

void ListenersFeedWidget::onCardGenerate(const QVariant& id)
{
    m_selectedName = id.toString();
    onGenerateAgent();
}

void ListenersFeedWidget::onCardDoubleClick(const QVariant& id)
{
    m_selectedName = id.toString();
    onEditListener();
}

void ListenersFeedWidget::onCardContextMenu(const QVariant& id, const QPoint& globalPos)
{
    if (m_cardList)
        m_cardList->ensureSelected(id);
    m_selectedName = id.toString();

    const int selCount = m_cardList ? m_cardList->selectedIds().size() : 1;
    if (selCount <= 1)
        return;

    oclero::qlementine::Menu ctxMenu;
    ctxMenu.addAction(QIcon(":/icons/tag"), "Set tag", this, &ListenersFeedWidget::onSetTag);
    ctxMenu.addSeparator();
    ctxMenu.addAction(QIcon(":/icons/stop"), "Pause", this, &ListenersFeedWidget::onPauseListener);
    ctxMenu.addAction(QIcon(":/icons/start"), "Resume", this, &ListenersFeedWidget::onResumeListener);
    ctxMenu.addSeparator();
    ctxMenu.addAction(QIcon(":/icons/delete"), "Remove", this, &ListenersFeedWidget::onRemoveListener);
    ctxMenu.exec(globalPos);
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
    dialogListener->SetEditMode(info.name, info.tags);
    dialogListener->Start();
}

void ListenersFeedWidget::onRemoveListener()
{
    QList<QVariant> ids = m_cardList ? m_cardList->selectedIds() : QList<QVariant>{};
    if (ids.isEmpty() && !m_selectedName.isEmpty())
        ids.append(m_selectedName);
    if (ids.isEmpty())
        return;

    QString prompt = (ids.size() == 1) ? QStringLiteral("Remove listener \"%1\"?").arg(ids.first().toString()) : QStringLiteral("Remove %1 selected listeners?").arg(ids.size());
    QMessageBox::StandardButton reply = QMessageBox::question( this, QStringLiteral("Remove Listener"), prompt, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    for (const QVariant& id : ids) {
        const ListenerData* l = findByName(id.toString());
        if (!l)
            continue;
        HttpReqListenerStopAsync(l->Name, l->ListenerRegName, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? QStringLiteral("Response timeout") : message);
        });
    }
}

void ListenersFeedWidget::onPauseListener()
{
    QList<QVariant> ids = m_cardList ? m_cardList->selectedIds() : QList<QVariant>{};
    if (ids.isEmpty() && !m_selectedName.isEmpty())
        ids.append(m_selectedName);
    for (const QVariant& id : ids) {
        const ListenerData* l = findByName(id.toString());
        if (!l)
            continue;
        HttpReqListenerPauseAsync(l->Name, l->ListenerRegName, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success) MessageError(message.isEmpty() ? QStringLiteral("Response timeout") : message);
        });
    }
}

void ListenersFeedWidget::onResumeListener()
{
    QList<QVariant> ids = m_cardList ? m_cardList->selectedIds() : QList<QVariant>{};
    if (ids.isEmpty() && !m_selectedName.isEmpty())
        ids.append(m_selectedName);
    for (const QVariant& id : ids) {
        const ListenerData* l = findByName(id.toString());
        if (!l)
            continue;
        HttpReqListenerResumeAsync(l->Name, l->ListenerRegName, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success) MessageError(message.isEmpty() ? QStringLiteral("Response timeout") : message);
        });
    }
}

void ListenersFeedWidget::onSetTag()
{
    QList<QVariant> ids = m_cardList ? m_cardList->selectedIds() : QList<QVariant>{};
    if (ids.isEmpty() && !m_selectedName.isEmpty())
        ids.append(m_selectedName);
    if (ids.isEmpty())
        return;

    QString seedTags;
    if (const ListenerData* first = findByName(ids.first().toString()))
        seedTags = first->Tags;

    bool inputOk = false;
    QString newTag = QInputDialog::getText(this, QStringLiteral("Set tags"), ids.size() == 1 ? QStringLiteral("New tag") : QStringLiteral("New tag for %1 listeners").arg(ids.size()), QLineEdit::Normal, seedTags, &inputOk);
    if (!inputOk)
        return;

    for (const QVariant& id : ids) {
        const ListenerData* l = findByName(id.toString());
        if (!l)
            continue;
        HttpReqListenerSetTagsAsync(l->Name, newTag, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success) MessageError(message.isEmpty() ? QStringLiteral("Response timeout") : message);
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
