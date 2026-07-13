#include <Agent/Agent.h>
#include <UI/Widgets/TunnelsFeedWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>
#include <Client/Requestor.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Utils/FontManager.h>
#include <MainAdaptix.h>

#include <oclero/qlementine/widgets/Menu.hpp>

#include <QPainter>
#include <QSortFilterProxyModel>

TunnelsFilterProxy::TunnelsFilterProxy(QObject* parent) : QSortFilterProxyModel(parent) {}

void TunnelsFilterProxy::setSearchText(const QString& text) { m_searchText = text; }

bool TunnelsFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    auto* srcModel = qobject_cast<FeedListModel*>(sourceModel());
    if (!srcModel || sourceRow < 0 || sourceRow >= srcModel->size())
        return true;

    const FeedRow& row = srcModel->rowAt(sourceRow);

    if (m_searchText.isEmpty())
        return true;

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

namespace TunnelsBlock {
    enum {
        Id    = 0,
        Main  = 1,
        Tags  = 2,
        Right = 3,
        Count = 4
    };
}

static QString formatBytes(qint64 bytes) {
    if (bytes < 1024)
        return QString("%1 B").arg(bytes);
    if (bytes < 1024*1024)
        return QString("%1 KB").arg(bytes/1024.0, 0, 'f', 1);
    if (bytes < 1024*1024*1024)
        return QString("%1 MB").arg(bytes/(1024.0*1024), 0, 'f', 1);
    return QString("%1 GB").arg(bytes/(1024.0*1024*1024), 0, 'f', 1);
}

static FeedRow tunnelToFeedRow(const TunnelData& t) {
    FeedRow row;
    row.resize(TunnelsBlock::Count);
    row.entityId = t.TunnelId;

    row[TunnelsBlock::Id] = QVariantMap{
        {"id", QString("#%1").arg(t.TunnelId)},
        {"badge", t.Type},
        {"date", QDateTime::fromSecsSinceEpoch(t.DateTimestamp).toString("dd/MM HH:mm:ss")}
    };

    QString endpoint;
    if (!t.Fhost.isEmpty() && !t.Fport.isEmpty())
        endpoint = QString("%1:%2 → %3:%4").arg(t.Interface, t.Port, t.Fhost, t.Fport);
    else
        endpoint = QString("%1:%2").arg(t.Interface, t.Port);

    QString second = QString("%1 @ %2 (%3) #%4").arg(t.Username, t.Computer, t.Process).arg(t.AgentId);

    row[TunnelsBlock::Main] = QVariantMap{{"main", endpoint}, {"submain", t.Info}, {"second", second}};

    QStringList tags;
    if (!t.Client.isEmpty())
        tags << QString("side: %1").arg(t.Client);
    row[TunnelsBlock::Tags] = QVariant(tags);

    QString traffic;
    if (t.BytesSent > 0 || t.BytesRecv > 0)
        traffic = QString("\u2191 %1     \u2502  \u2193 %2").arg(formatBytes(t.BytesSent), formatBytes(t.BytesRecv));

    row[TunnelsBlock::Right] = QVariantMap{{"main", QString()}, {"second", traffic}, {"status", "Active"}, {"statusType", "success"}};
    row.isDead = false;
    return row;
}

static ListFeedDelegate* createTunnelsDelegate(QObject* parent) {
    auto* d = new ListFeedDelegate(parent);
    d->addBlock(new IdBadgeBlock());
    d->addBlock(new MainBlock());
    d->addBlock(new TagsBlock());
    d->addBlock(new StatusBlock());
    return d;
}



TunnelsFeedWidget::TunnelsFeedWidget(AdaptixWidget* w) : ListFeedWidget(w), m_adaptixWidget(w)
{
    feedBlockModel = new FeedListModel(this);
    auto* delegate = createTunnelsDelegate(this);
    delegate->setFeedModel(feedBlockModel);

    filterProxy = new TunnelsFilterProxy(this);
    filterProxy->setSourceModel(feedBlockModel);

    setModel(feedBlockModel);
    setDelegate(delegate);
    setFilterModel(filterProxy);
    rebuildModelChain();

    enableSearch(true);
    finalizeSearchWidget();

    enableCompactSwitch(true);
    setBlockGap(12);
    setTagSize(11, 20);
    setIconSizes(22, 18);

    dockWidget = new KDDockWidgets::QtWidgets::DockWidget( "TunnelsFeed:Dock-" + w->GetProfile()->GetProject(), KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    dockWidget->setTitle("Tunnels");
    dockWidget->setWidget(this);
    dockWidget->setIcon(QIcon(":/icons/vpn"), KDDockWidgets::IconPlace::TabBar);

    connect(treeView(), &QTreeView::customContextMenuRequested, this, &TunnelsFeedWidget::handleFeedMenu);
}

TunnelsFeedWidget::~TunnelsFeedWidget() = default;

KDDockWidgets::QtWidgets::DockWidget* TunnelsFeedWidget::dock() { return dockWidget; }

void TunnelsFeedWidget::SetUpdatesEnabled(bool enabled)
{
    treeView()->setUpdatesEnabled(enabled);
}

void TunnelsFeedWidget::Clear()
{
    if (feedBlockModel)
        feedBlockModel->clear();
}

void TunnelsFeedWidget::AddTunnelItem(const TunnelData& newTunnel)
{
    if (!feedBlockModel)
        return;

    for (int i = 0; i < feedBlockModel->size(); ++i) {
        const FeedRow& r = feedBlockModel->rowAt(i);
        if (r.entityId == newTunnel.TunnelId) {
            feedBlockModel->updateRow(i, tunnelToFeedRow(newTunnel));
            return;
        }
    }

    feedBlockModel->insertRow(0, tunnelToFeedRow(newTunnel));
}

void TunnelsFeedWidget::EditTunnelItem(qint64 tunnelId, const QString& info)
{
    if (!feedBlockModel)
        return;
    for (int i = 0; i < feedBlockModel->size(); ++i) {
        const FeedRow& r = feedBlockModel->rowAt(i);
        if (r.entityId == tunnelId) {
            FeedRow newRow = r;
            QVariantMap mainMap = r.blockData[TunnelsBlock::Main].toMap();
            mainMap["submain"] = info;
            newRow[TunnelsBlock::Main] = mainMap;
            feedBlockModel->updateRow(i, newRow);
            return;
        }
    }
}

void TunnelsFeedWidget::RemoveTunnelItem(qint64 tunnelId)
{
    if (!feedBlockModel)
        return;
    for (int i = 0; i < feedBlockModel->size(); ++i) {
        const FeedRow& r = feedBlockModel->rowAt(i);
        if (r.entityId == tunnelId) {
            feedBlockModel->removeRow(i);
            return;
        }
    }
}

void TunnelsFeedWidget::handleFeedMenu(const QPoint& pos)
{
    QModelIndex index = prepareContextMenuSelection(pos);
    if (!index.isValid())
        return;

    qint64 tunnelId = proxyModel()->data(index, Qt::UserRole).toLongLong();
    if (tunnelId == 0)
        return;

    oclero::qlementine::Menu ctxMenu;
    ctxMenu.addAction("Set info", this, &TunnelsFeedWidget::actionSetInfo);
    ctxMenu.addSeparator();
    ctxMenu.addAction("Stop tunnel", this, &TunnelsFeedWidget::actionStopTunnel);

    ctxMenu.exec(treeView()->viewport()->mapToGlobal(pos));
}

void TunnelsFeedWidget::actionSetInfo()
{
    QModelIndex idx = treeView()->currentIndex();
    if (!idx.isValid())
        return;
    qint64 tunnelId = proxyModel()->data(idx, Qt::UserRole).toLongLong();
    if (tunnelId == 0)
        return;

    QString currentInfo;
    for (int i = 0; i < feedBlockModel->size(); ++i) {
        const FeedRow& r = feedBlockModel->rowAt(i);
        if (r.entityId == tunnelId) {
            currentInfo = r.blockData[TunnelsBlock::Main].toMap()["submain"].toString();
            break;
        }
    }

    bool ok;
    QString newInfo = QInputDialog::getText(this, "Set Tunnel Info", "Info:", QLineEdit::Normal, currentInfo, &ok);
    if (ok) {
        HttpReqTunnelSetInfoAsync(tunnelId, newInfo, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success) MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void TunnelsFeedWidget::actionStopTunnel()
{
    QModelIndex idx = treeView()->currentIndex();
    if (!idx.isValid())
        return;
    qint64 tunnelId = proxyModel()->data(idx, Qt::UserRole).toLongLong();
    if (tunnelId == 0)
        return;

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Stop Tunnel", "Stop tunnel #" + QString::number(tunnelId) + "?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    HttpReqTunnelStopAsync(tunnelId, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success) MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void TunnelsFeedWidget::onFilterChanged()
{
    if (filterProxy) {
        filterProxy->setSearchText( searchInput() ? searchInput()->text() : QString() );
        filterProxy->invalidate();
    }
}
