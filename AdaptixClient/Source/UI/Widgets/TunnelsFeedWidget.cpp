#include <Agent/Agent.h>
#include <UI/Widgets/TunnelsFeedWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>
#include <Client/Requestor.h>
#include <Utils/CustomElements/ControlCard.h>
#include <Utils/FontManager.h>
#include <Utils/Convert.h>
#include <MainAdaptix.h>

#include <oclero/qlementine/widgets/Menu.hpp>
#include <oclero/qlementine/widgets/LineEdit.hpp>

#include <QInputDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QDateTime>
#include <QRegularExpression>
#include <QIcon>

static QString formatBytes(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024)
        return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 1);
}

TunnelsFeedWidget::TunnelsFeedWidget(AdaptixWidget* w) : QWidget(w), m_adaptixWidget(w)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* toolbar = new QWidget(this);
    auto* tb = new QHBoxLayout(toolbar);
    tb->setContentsMargins(8, 6, 8, 6);
    tb->setSpacing(8);

    auto* searchEdit = new oclero::qlementine::LineEdit(toolbar);
    searchEdit->setIcon(QIcon(":/icons/search"));
    searchEdit->setPlaceholderText("Search tunnels...");
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setMinimumWidth(160);
    searchEdit->setFixedHeight(FontManager::instance().typography().controlHeight);
    m_search = searchEdit;
    tb->addWidget(m_search, 1);

    m_cardList = new ControlCardList(this);

    root->addWidget(toolbar, 0);
    root->addWidget(m_cardList, 1);

    connect(m_search, &QLineEdit::textChanged, this, &TunnelsFeedWidget::onSearchChanged);
    connect(m_cardList, &ControlCardList::primaryActionClicked, this, &TunnelsFeedWidget::onCardPrimary);
    connect(m_cardList, &ControlCardList::deleteClicked, this, &TunnelsFeedWidget::onCardDelete);
    connect(m_cardList, &ControlCardList::doubleClicked, this, &TunnelsFeedWidget::onCardDoubleClick);
    connect(m_cardList, &ControlCardList::selectionChanged, this, &TunnelsFeedWidget::onCardSelected);
    connect(m_cardList, &ControlCardList::contextMenuRequested, this, &TunnelsFeedWidget::onCardContextMenu);

    dockWidget = new KDDockWidgets::QtWidgets::DockWidget( "TunnelsFeed:Dock-" + w->GetProfile()->GetProject(), KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    dockWidget->setTitle("Tunnels");
    dockWidget->setWidget(this);
    dockWidget->setIcon(QIcon(":/icons/vpn"), KDDockWidgets::IconPlace::TabBar);
}

TunnelsFeedWidget::~TunnelsFeedWidget() = default;

KDDockWidgets::QtWidgets::DockWidget* TunnelsFeedWidget::dock() { return dockWidget; }

void TunnelsFeedWidget::SetUpdatesEnabled(bool enabled)
{
    setUpdatesEnabled(enabled);
    if (m_cardList)
        m_cardList->setUpdatesEnabled(enabled);
}

void TunnelsFeedWidget::Clear()
{
    m_items.clear();
    m_selectedId = 0;
    if (m_cardList)
        m_cardList->clear();
}

static QString formatHostPort(const QString& host, const QString& port)
{
    if (!host.isEmpty() && !port.isEmpty())
        return host + " :" + port;
    if (!host.isEmpty())
        return host;
    if (!port.isEmpty())
        return ":" + port;
    return "—";
}

static QString tunnelTitleDisplay(const QString& rawType)
{
    const QString t = rawType.trimmed();
    const QString key = t.toLower();
    if (key.contains("socks5") && key.contains("auth"))
        return "Socks5 Auth";
    if (key.contains("socks5"))
        return "Socks5";
    if (key.contains("socks4"))
        return "Socks4";
    if (key.contains("local") && key.contains("port"))
        return "LportFwd";
    if (key.contains("reverse") && key.contains("port"))
        return "RportFwd";
    if (t.isEmpty())
        return "Tunnel";

    QStringList parts = t.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (QString& p : parts) {
        if (p.size() > 1)
            p = p.left(1).toUpper() + p.mid(1).toLower();
        else
            p = p.toUpper();
    }
    return parts.join(' ');
}

static QString currentOperator(const AdaptixWidget* w)
{
    if (!w || !w->GetProfile())
        return {};
    return w->GetProfile()->GetUsername().trimmed();
}

static bool canControlTunnel(const TunnelData& t, const QString& me)
{
    const QString owner = t.Client.trimmed();
    if (owner.isEmpty())
        return true;
    return !me.isEmpty() && QString::compare(owner, me, Qt::CaseSensitive) == 0;
}

ControlCardData TunnelsFeedWidget::toCard(const TunnelData& t) const
{
    ControlCardData d;
    d.id = t.TunnelId;
    d.bodyLayout = ControlCard::BodyThreeLine;
    d.contentStyle = ControlCard::StyleTunnel;

    d.title = tunnelTitleDisplay(t.Type);
    if (!t.Client.trimmed().isEmpty())
        d.titleSuffix = t.Client.trimmed();

    d.primaryPrefix = "listen";
    d.primary = formatHostPort(t.Interface.isEmpty() ? "*" : t.Interface, t.Port);

    if (!t.Fhost.isEmpty() || !t.Fport.isEmpty()) {
        d.detailPrefix = "to";
        d.detail = formatHostPort(t.Fhost.isEmpty() ? "*" : t.Fhost, t.Fport);
    }

    if (!t.Info.trimmed().isEmpty())
        d.sideText = t.Info.trimmed();

    if (t.DateTimestamp > 0)
        d.dateText = UnixTimestampGlobalToStringLocalSmall(t.DateTimestamp);

    d.secondaryLead.clear();
    d.secondaryPrefix = "AGENT";

    QStringList rest;
    if (t.AgentId != 0)
        rest << QStringLiteral("#%1").arg(t.AgentId);
    {
        const QString user = t.Username.trimmed();
        const QString host = t.Computer.trimmed();
        if (!user.isEmpty() || !host.isEmpty()) {
            rest << ((user.isEmpty() ? "?" : user) + "@" + (host.isEmpty() ? "?" : host));
        }
    }
    if (!t.Process.isEmpty())
        rest << t.Process;
    if (rest.isEmpty()) {
        d.secondary.clear();
    } else if (rest.size() == 1) {
        d.secondary = rest.first();
    } else if (t.AgentId != 0) {
        d.secondary = rest.first() + " | " + rest.mid(1).join(' ');
    } else {
        d.secondary = rest.join(' ');
    }

    if (t.BytesSent > 0 || t.BytesRecv > 0) {
        d.tertiaryPrefix = "traffic";
        d.tertiary = QStringLiteral("↑ %1   ↓ %2").arg(formatBytes(t.BytesSent), formatBytes(t.BytesRecv));
    }

    d.active = t.Active;
    d.status = t.Active ? "Active" : "Paused";

    const bool canCtrl = canControlTunnel(t, currentOperator(m_adaptixWidget));
    d.showPrimaryAction = canCtrl;
    if (canCtrl) {
        if (t.Active) {
            d.primaryAction = ControlCard::ActionStop;
            d.primaryActionLabel = "Pause";
        } else {
            d.primaryAction = ControlCard::ActionStart;
            d.primaryActionLabel = "Resume";
        }
        d.showDelete = true;
        d.deleteActionLabel = "Remove";
    } else {
        d.primaryAction = ControlCard::ActionNone;
        d.showDelete = false;
    }
    return d;
}

bool TunnelsFeedWidget::matchesFilter(const TunnelData& t) const
{
    if (!m_search)
        return true;
    const QString q = m_search->text().trimmed().toLower();
    if (q.isEmpty())
        return true;
    const QString hay = (t.Type + t.Interface + t.Port + t.Fhost + t.Fport + t.Username + t.Computer + t.Process + t.Info + t.Client + QString::number(t.TunnelId) + QString::number(t.AgentId)).toLower();
    return hay.contains(q);
}

void TunnelsFeedWidget::rebuildVisible()
{
    if (!m_cardList)
        return;
    QVector<ControlCardData> cards;
    cards.reserve(m_items.size());
    for (const auto& t : m_items) {
        if (matchesFilter(t))
            cards.append(toCard(t));
    }
    m_cardList->setCards(cards);
    if (m_selectedId != 0)
        m_cardList->setSelectedId(m_selectedId);
}

TunnelData* TunnelsFeedWidget::findById(qint64 id)
{
    for (auto& t : m_items) {
        if (t.TunnelId == id)
            return &t;
    }
    return nullptr;
}

const TunnelData* TunnelsFeedWidget::findById(qint64 id) const
{
    for (const auto& t : m_items) {
        if (t.TunnelId == id)
            return &t;
    }
    return nullptr;
}

void TunnelsFeedWidget::AddTunnelItem(const TunnelData& newTunnel)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].TunnelId == newTunnel.TunnelId) {
            m_items[i] = newTunnel;
            if (m_cardList && matchesFilter(newTunnel))
                m_cardList->upsertCard(toCard(newTunnel));
            else
                rebuildVisible();
            return;
        }
    }
    m_items.prepend(newTunnel);
    if (matchesFilter(newTunnel) && m_cardList)
        m_cardList->upsertCard(toCard(newTunnel));
}

void TunnelsFeedWidget::EditTunnelItem(qint64 tunnelId, const QString& info)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].TunnelId == tunnelId) {
            m_items[i].Info = info;
            if (m_cardList && matchesFilter(m_items[i]))
                m_cardList->upsertCard(toCard(m_items[i]));
            else
                rebuildVisible();
            return;
        }
    }
}

void TunnelsFeedWidget::SetTunnelActive(qint64 tunnelId, bool active)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].TunnelId == tunnelId) {
            m_items[i].Active = active;
            if (m_cardList && matchesFilter(m_items[i]))
                m_cardList->upsertCard(toCard(m_items[i]));
            else
                rebuildVisible();
            return;
        }
    }
}

void TunnelsFeedWidget::RemoveTunnelItem(qint64 tunnelId)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].TunnelId == tunnelId) {
            m_items.removeAt(i);
            break;
        }
    }
    if (m_selectedId == tunnelId)
        m_selectedId = 0;
    if (m_cardList)
        m_cardList->removeCard(tunnelId);
}

void TunnelsFeedWidget::onSearchChanged(const QString&)
{
    rebuildVisible();
}

void TunnelsFeedWidget::onCardSelected(const QVariant& id)
{
    m_selectedId = id.toLongLong();
}

void TunnelsFeedWidget::onCardPrimary(const QVariant& id)
{
    m_selectedId = id.toLongLong();
    const TunnelData* t = findById(m_selectedId);
    if (!t || !canControlTunnel(*t, currentOperator(m_adaptixWidget)))
        return;
    if (t->Active)
        actionPauseTunnel();
    else
        actionResumeTunnel();
}

void TunnelsFeedWidget::onCardDelete(const QVariant& id)
{
    m_selectedId = id.toLongLong();
    const TunnelData* t = findById(m_selectedId);
    if (!t || !canControlTunnel(*t, currentOperator(m_adaptixWidget)))
        return;
    actionStopTunnel();
}

void TunnelsFeedWidget::onCardDoubleClick(const QVariant& id)
{
    m_selectedId = id.toLongLong();
    const TunnelData* t = findById(m_selectedId);
    if (!t || !canControlTunnel(*t, currentOperator(m_adaptixWidget)))
        return;
    actionSetInfo();
}

void TunnelsFeedWidget::onCardContextMenu(const QVariant& id, const QPoint& globalPos)
{
    if (m_cardList)
        m_cardList->ensureSelected(id);
    m_selectedId = id.toLongLong();

    const QList<QVariant> ids = m_cardList ? m_cardList->selectedIds() : QList<QVariant>{};
    if (ids.size() <= 1)
        return;

    const QString me = currentOperator(m_adaptixWidget);
    bool anyCtrl = false;
    for (const QVariant& v : ids) {
        if (const TunnelData* t = findById(v.toLongLong())) {
            if (canControlTunnel(*t, me)) {
                anyCtrl = true;
                break;
            }
        }
    }
    if (!anyCtrl)
        return;

    oclero::qlementine::Menu ctxMenu;
    ctxMenu.addAction(QIcon(":/icons/tag"), "Set info", this, &TunnelsFeedWidget::actionSetInfo);
    ctxMenu.addSeparator();
    ctxMenu.addAction(QIcon(":/icons/stop"), "Pause", this, &TunnelsFeedWidget::actionPauseTunnel);
    ctxMenu.addAction(QIcon(":/icons/start"), "Resume", this, &TunnelsFeedWidget::actionResumeTunnel);
    ctxMenu.addSeparator();
    ctxMenu.addAction(QIcon(":/icons/delete"), "Remove", this, &TunnelsFeedWidget::actionStopTunnel);
    ctxMenu.exec(globalPos);
}

QList<qint64> TunnelsFeedWidget::controllableSelectedIds() const
{
    QList<qint64> out;
    const QString me = currentOperator(m_adaptixWidget);
    QList<QVariant> ids = m_cardList ? m_cardList->selectedIds() : QList<QVariant>{};
    if (ids.isEmpty() && m_selectedId != 0)
        ids.append(m_selectedId);
    for (const QVariant& v : ids) {
        const qint64 tid = v.toLongLong();
        if (tid == 0)
            continue;
        if (const TunnelData* t = findById(tid)) {
            if (canControlTunnel(*t, me))
                out.append(tid);
        }
    }
    return out;
}

void TunnelsFeedWidget::actionSetInfo()
{
    const QList<qint64> ids = controllableSelectedIds();
    if (ids.isEmpty())
        return;

    QString currentInfo;
    if (const TunnelData* t = findById(ids.first()))
        currentInfo = t->Info;

    bool ok = false;
    QString newInfo = QInputDialog::getText(this, "Set Tunnel Info", "Info:", QLineEdit::Normal, currentInfo, &ok);
    if (!ok)
        return;

    for (qint64 tid : ids) {
        HttpReqTunnelSetInfoAsync(tid, newInfo, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void TunnelsFeedWidget::actionPauseTunnel()
{
    if (!m_adaptixWidget || !m_adaptixWidget->GetProfile())
        return;
    for (qint64 tid : controllableSelectedIds()) {
        HttpReqTunnelPauseAsync(tid, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void TunnelsFeedWidget::actionResumeTunnel()
{
    if (!m_adaptixWidget || !m_adaptixWidget->GetProfile())
        return;
    for (qint64 tid : controllableSelectedIds()) {
        HttpReqTunnelResumeAsync(tid, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void TunnelsFeedWidget::actionStopTunnel()
{
    const QList<qint64> ids = controllableSelectedIds();
    if (ids.isEmpty())
        return;

    QString prompt = (ids.size() == 1)
        ? QStringLiteral("Remove tunnel #%1 permanently?").arg(ids.first())
        : QStringLiteral("Remove %1 selected tunnels permanently?").arg(ids.size());
    QMessageBox::StandardButton reply = QMessageBox::question( this, "Remove Tunnel", prompt, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    for (qint64 tid : ids) {
        HttpReqTunnelStopAsync(tid, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}
