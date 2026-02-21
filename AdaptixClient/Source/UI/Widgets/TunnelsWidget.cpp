#include <UI/Widgets/TunnelsWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/TunnelEndpoint.h>
#include <Agent/Agent.h>
#include <UI/Graph/GraphItem.h>

REGISTER_DOCK_WIDGET(TunnelsWidget, "Tunnels", true)

TunnelsWidget::TunnelsWidget(AdaptixWidget* w) : DockTab("Tunnels", w->GetProfile()->GetProject(), ":/icons/vpn")
{
    this->adaptixWidget = w;

    this->createUI();

    connect(tableView, &QTableView::customContextMenuRequested, this, &TunnelsWidget::handleTunnelsMenu);
    connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &selected, const QItemSelection &deselected){
            Q_UNUSED(selected)
            Q_UNUSED(deselected)
            if (!inputFilter->hasFocus())
                tableView->setFocus();
    });
    connect(inputFilter, &QLineEdit::textChanged,   this, &TunnelsWidget::onFilterUpdate);
    connect(inputFilter, &QLineEdit::returnPressed, this, [this]() { proxyModel->setTextFilter(inputFilter->text()); });
    connect(hideButton,  &ClickableLabel::clicked,  this, &TunnelsWidget::toggleSearchPanel);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), this);
    shortcutSearch->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &TunnelsWidget::toggleSearchPanel);

    auto shortcutEsc = new QShortcut(QKeySequence(Qt::Key_Escape), inputFilter);
    shortcutEsc->setContext(Qt::WidgetShortcut);
    connect(shortcutEsc, &QShortcut::activated, this, [this]() { searchWidget->setVisible(false); });

    this->dockWidget->setWidget(this);
}

TunnelsWidget::~TunnelsWidget() = default;

void TunnelsWidget::SetUpdatesEnabled(const bool enabled)
{
    tableView->setUpdatesEnabled(enabled);
}

void TunnelsWidget::createUI()
{
    auto horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    searchWidget = new QWidget(this);
    searchWidget->setVisible(false);

    inputFilter = new QLineEdit(searchWidget);
    inputFilter->setPlaceholderText("filter: (socks | forward) & ^(test)");
    inputFilter->setMaximumWidth(300);

    autoSearchCheck = new QCheckBox("auto", searchWidget);
    autoSearchCheck->setChecked(true);
    autoSearchCheck->setToolTip("Auto search on text change. If unchecked, press Enter to search.");

    hideButton = new ClickableLabel("  x  ");
    hideButton->setCursor(Qt::PointingHandCursor);

    searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(0, 4, 0, 0);
    searchLayout->setSpacing(4);
    searchLayout->addWidget(inputFilter);
    searchLayout->addWidget(autoSearchCheck);
    searchLayout->addSpacing(8);
    searchLayout->addWidget(hideButton);
    searchLayout->addSpacerItem(horizontalSpacer);

    tunnelsModel = new TunnelsTableModel(this);
    proxyModel = new TunnelsFilterProxyModel(this);
    proxyModel->setSourceModel(tunnelsModel);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    tableView = new QTableView(this);
    tableView->setModel(proxyModel);
    tableView->setHorizontalHeader(new BoldHeaderView(Qt::Horizontal, tableView));
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

    tableView->hideColumn(TUC_TunnelId);
    tableView->setItemDelegate(new PaddingDelegate(tableView));

    mainGridLayout = new QGridLayout(this);
    mainGridLayout->setContentsMargins(0, 0, 0, 0);
    mainGridLayout->addWidget(searchWidget, 0, 0, 1, 1);
    mainGridLayout->addWidget(tableView,    1, 0, 1, 1);
}

void TunnelsWidget::Clear() const
{
    {
        QWriteLocker locker(&adaptixWidget->TunnelsLock);
        adaptixWidget->Tunnels.clear();
    }
    tunnelsModel->clear();
    inputFilter->clear();
}

void TunnelsWidget::AddTunnelItem(const TunnelData &newTunnel) const
{
    if (tunnelsModel->contains(newTunnel.TunnelId))
        return;

    {
        QWriteLocker locker(&adaptixWidget->TunnelsLock);
        adaptixWidget->Tunnels.push_back(newTunnel);
    }
    tunnelsModel->add(newTunnel);

    tableView->horizontalHeader()->setSectionResizeMode(TUC_TunnelId,  QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(TUC_AgentId,   QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(TUC_Interface, QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(TUC_Port,      QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(TUC_Fhost,     QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(TUC_Fport,     QHeaderView::ResizeToContents);

    {
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        if (adaptixWidget->AgentsMap.contains(newTunnel.AgentId)) {
            Agent* agent = adaptixWidget->AgentsMap[newTunnel.AgentId];
            if (agent && agent->graphItem) {
                TunnelMarkType markType = newTunnel.Client.isEmpty() ? TunnelMarkServer : TunnelMarkClient;
                agent->graphItem->AddTunnel(markType);
            }
        }
    }
}

void TunnelsWidget::EditTunnelItem(const QString &tunnelId, const QString &info) const
{
    {
        QWriteLocker locker(&adaptixWidget->TunnelsLock);
        for (int i = 0; i < adaptixWidget->Tunnels.size(); i++) {
            if (adaptixWidget->Tunnels[i].TunnelId == tunnelId) {
                adaptixWidget->Tunnels[i].Info = info;
                break;
            }
        }
    }

    tunnelsModel->updateInfo(tunnelId, info);
}

void TunnelsWidget::RemoveTunnelItem(const QString &tunnelId) const
{
    QString agentId;
    TunnelMarkType markType = TunnelMarkNone;
    {
        QWriteLocker locker(&adaptixWidget->TunnelsLock);
        for (int i = 0; i < adaptixWidget->Tunnels.size(); i++) {
            if (adaptixWidget->Tunnels[i].TunnelId == tunnelId) {
                agentId = adaptixWidget->Tunnels[i].AgentId;
                markType = adaptixWidget->Tunnels[i].Client.isEmpty() ? TunnelMarkServer : TunnelMarkClient;
                adaptixWidget->Tunnels.erase(adaptixWidget->Tunnels.begin() + i);
                break;
            }
        }
    }

    if (!agentId.isEmpty()) {
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        if (adaptixWidget->AgentsMap.contains(agentId)) {
            Agent* agent = adaptixWidget->AgentsMap[agentId];
            if (agent && agent->graphItem && markType != TunnelMarkNone)
                agent->graphItem->RemoveTunnel(markType);
        }
    }

    tunnelsModel->remove(tunnelId);

    if (adaptixWidget->ClientTunnels.contains(tunnelId)) {
        auto tunnel = adaptixWidget->ClientTunnels[tunnelId];
        adaptixWidget->ClientTunnels.remove(tunnelId);
        tunnel->Stop();
        delete tunnel;
    }
}

/// Slots

void TunnelsWidget::toggleSearchPanel() const
{
    if (searchWidget->isVisible()) {
        searchWidget->setVisible(false);
    } else {
        searchWidget->setVisible(true);
        inputFilter->setFocus();
    }
}

void TunnelsWidget::onFilterUpdate() const
{
    if (autoSearchCheck->isChecked()) {
        proxyModel->setTextFilter(inputFilter->text());
    }
    inputFilter->setFocus();
}

void TunnelsWidget::handleTunnelsMenu(const QPoint &pos) const
{
    QMenu tunnelsMenu = QMenu();

    tunnelsMenu.addAction("Set info", this, &TunnelsWidget::actionSetInfo);
    tunnelsMenu.addAction("Stop",     this, &TunnelsWidget::actionStopTunnel);

    QPoint globalPos = tableView->mapToGlobal(pos);
    tunnelsMenu.exec(globalPos);
}

void TunnelsWidget::actionSetInfo() const
{
    if (tableView->selectionModel()->selectedRows().empty())
        return;

    QModelIndex currentIndex = tableView->currentIndex();
    QModelIndex sourceIndex = proxyModel->mapToSource(currentIndex);
    if (!sourceIndex.isValid())
        return;

    QString tunnelId = tunnelsModel->data(tunnelsModel->index(sourceIndex.row(), TUC_TunnelId), Qt::DisplayRole).toString();
    QString info     = tunnelsModel->data(tunnelsModel->index(sourceIndex.row(), TUC_Info),     Qt::DisplayRole).toString();

    bool inputOk;
    QString newInfo = QInputDialog::getText(nullptr, "Set info", "Info:", QLineEdit::Normal, info, &inputOk);
    if (inputOk) {
        HttpReqTunnelSetInfoAsync(tunnelId, newInfo, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void TunnelsWidget::actionStopTunnel() const
{
    if (tableView->selectionModel()->selectedRows().empty())
        return;

    QModelIndex currentIndex = tableView->currentIndex();
    QModelIndex sourceIndex = proxyModel->mapToSource(currentIndex);
    if (!sourceIndex.isValid())
        return;

    QString tunnelId = tunnelsModel->data(tunnelsModel->index(sourceIndex.row(), TUC_TunnelId), Qt::DisplayRole).toString();

    if (adaptixWidget->ClientTunnels.contains(tunnelId)) {
        auto tunnel = adaptixWidget->ClientTunnels[tunnelId];
        adaptixWidget->ClientTunnels.remove(tunnelId);
        tunnel->Stop();
        delete tunnel;
    }

    HttpReqTunnelStopAsync(tunnelId, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}
