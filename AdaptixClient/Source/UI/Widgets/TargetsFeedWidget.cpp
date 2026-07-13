#include <UI/Widgets/TargetsFeedWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Dialogs/DialogTarget.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/Settings.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Utils/NonBlockingDialogs.h>
#include <MainAdaptix.h>

#include <oclero/qlementine/widgets/Menu.hpp>

#include <QPainter>
#include <QClipboard>
#include <QFileDialog>
#include <QInputDialog>


namespace TargetsBlock { enum { Icon = 0, Id = 1, Text = 2, Main = 3, Tags = 4, Right = 5, Count = 6 }; }

namespace TF { enum { Icon = 0, TargetId, Created, Computer, Domain, Address, Os, Info, Tags, Status, Count }; }

static bool targetsCol(int col)
{
    return GlobalClient && GlobalClient->settings && GlobalClient->settings->data.TargetsTableColumns[col];
}

static QIcon targetsOsIconFor(int os, bool owned, bool alive)
{
    switch (os) {
        case OS_WINDOWS:
            if (owned) return QIcon(":/icons/os_win_red");
            if (alive) return QIcon(":/icons/os_win_blue");
            return QIcon(":/icons/os_win_grey");
        case OS_LINUX:
            if (owned) return QIcon(":/icons/os_linux_red");
            if (alive) return QIcon(":/icons/os_linux_blue");
            return QIcon(":/icons/os_linux_grey");
        case OS_MAC:
            if (owned) return QIcon(":/icons/os_mac_red");
            if (alive) return QIcon(":/icons/os_mac_blue");
            return QIcon(":/icons/os_mac_grey");
        default:
            return QIcon();
    }
}

static FeedRow targetToFeedRow(const TargetData& t)
{
    FeedRow row;
    row.resize(TargetsBlock::Count);
    row.entityId = t.TargetId;

    if (targetsCol(TF::Icon))
        row[TargetsBlock::Icon] = targetsOsIconFor(t.Os, !t.Agents.isEmpty(), t.Alive);

    QStringList tagList = t.Tag.split(",", Qt::SkipEmptyParts);
    row[TargetsBlock::Id] = QVariantMap{
        {"id", targetsCol(TF::TargetId) ? QString("#%1").arg(t.TargetId) : QString()},
        {"idNum", t.TargetId},
        {"date", targetsCol(TF::Created) ? QDateTime::fromSecsSinceEpoch(t.DateTimestamp).toString("dd/MM HH:mm:ss") : QString()},
        {"firstTag", tagList.isEmpty() ? QString() : tagList.first().toLower()}
    };

    QString mainText;
    if (targetsCol(TF::Computer) && !t.Computer.isEmpty())
        mainText = t.Computer;
    if (targetsCol(TF::Domain) && !t.Domain.isEmpty()) {
        if (!mainText.isEmpty())
            mainText += " @ " + t.Domain;
        else
            mainText = t.Domain;
    }

    QStringList secondParts;
    if (targetsCol(TF::Os) && !t.OsDesc.isEmpty())
        secondParts << t.OsDesc;
    if (targetsCol(TF::Info) && !t.Info.isEmpty())
        secondParts << t.Info;

    row[TargetsBlock::Main] = QVariantMap{
            {"main", mainText},
            {"submain", QString()},
            {"second", secondParts.join(" | ")},
            {"computer", t.Computer.toLower()},
            {"domain", t.Domain.toLower()},
            {"osDesc", t.OsDesc.toLower()},
            {"osNum", t.Os}
    };

    row[TargetsBlock::Text] = QVariantMap{
            {"main", targetsCol(TF::Address) ? t.Address : QString()},
            {"second", QString()},
            {"address", t.Address.toLower()}
    };

    row[TargetsBlock::Tags] = targetsCol(TF::Tags) ? tagList : QStringList();

    QString status, statusType;
    if (targetsCol(TF::Status)) {
        status = t.Alive ? "Alive" : "Offline";
        statusType = t.Alive ? "success" : "error";
    }
    row[TargetsBlock::Right] = QVariantMap{
        {"main", QString()},
        {"second", QString()},
        {"status", status},
        {"statusType", statusType},
        {"dateNum", t.DateTimestamp}
    };

    row.isDead = false;
    return row;
}

static ListFeedDelegate* createTargetsDelegate(QObject* parent)
{
    auto* d = new ListFeedDelegate(parent);
    d->addBlock(new IconBlock());
    d->addBlock(new IdBadgeBlock());
    d->addBlock(new TextBlock());
    d->addBlock(new MainBlock());
    d->addBlock(new TagsBlock());
    d->addBlock(new StatusBlock());
    d->addBlock(new GroupHeaderBlock());
    return d;
}



TargetsFeedWidget::TargetsFeedWidget(AdaptixWidget* w) : ListFeedWidget(w), m_adaptixWidget(w)
{
    feedBlockModel = new FeedListModel(this);
    auto* delegate = createTargetsDelegate(this);
    delegate->setFeedModel(feedBlockModel);

    setModel(feedBlockModel);
    setDelegate(delegate);
    rebuildModelChain();

    enableSearch(true);
    enableAutoCheck(true);
    enableFilterCombo(true, "All domains");
    enableSortingCombo(true, {"No sorting", "Date", "Computer", "Domain", "Address", "OS", "Tag"});
    finalizeSearchWidget();

    enableCompactSwitch(true);
    if (GlobalClient && GlobalClient->settings)
        setCompactMode(GlobalClient->settings->data.TargetsCompactMode);
    setBlockGap(12);
    setTagSize(11, 20);
    setIconSizes(22, 18);

    enablePagination(true);

    auto* addBtn = new QPushButton("+ Add Target", this);
    connect(addBtn, &QPushButton::clicked, this, &TargetsFeedWidget::onCreateTarget);
    addToolbarWidgetAfter(addBtn);

    dockWidget = new KDDockWidgets::QtWidgets::DockWidget( "TargetsFeed:Dock-" + w->GetProfile()->GetProject(), KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    dockWidget->setTitle("Targets");
    dockWidget->setWidget(this);
    dockWidget->setIcon(QIcon(":/icons/devices"), KDDockWidgets::IconPlace::TabBar);

    setupPagination();

    connect(treeView(), &QTreeView::customContextMenuRequested, this, &TargetsFeedWidget::handleFeedMenu);
    connect(treeView(), &QTreeView::doubleClicked, this, &TargetsFeedWidget::onItemDoubleClicked);
}

TargetsFeedWidget::~TargetsFeedWidget() = default;

KDDockWidgets::QtWidgets::DockWidget* TargetsFeedWidget::dock() { return dockWidget; }

void TargetsFeedWidget::setupPagination()
{
    pageHelper = new PagedTableHelper(m_adaptixWidget->GetProfile(), "/targets/list", this);
    pageHelper->setPageSize(paginationBar()->pageSize());

    connect(pageHelper, &PagedTableHelper::pageReady, this, &TargetsFeedWidget::onPageReady);
    connect(pageHelper, &PagedTableHelper::errorOccurred, this, &TargetsFeedWidget::onPageError);
    connect(pageHelper, &PagedTableHelper::loadingChanged, this, [this](bool loading) {
        paginationBar()->setLoading(loading);
        treeView()->setEnabled(!loading);
    });
    connect(paginationBar(), &PaginationBar::prevClicked, this, [this]() {
        m_offset = qMax(0, m_offset - paginationBar()->pageSize());
        loadCurrentPage();
    });
    connect(paginationBar(), &PaginationBar::nextClicked, this, [this]() {
        m_offset += paginationBar()->pageSize();
        loadCurrentPage();
    });
    connect(paginationBar(), &PaginationBar::pageSizeChanged, this, [this](int size) {
        pageHelper->setPageSize(size);
        m_offset = 0;
        loadCurrentPage();
    });
}

void TargetsFeedWidget::loadCurrentPage()
{
    if (searchInput())
        pageHelper->setParam("q", searchInput()->text());
    if (filterCombo() && filterCombo()->currentIndex() > 0)
        pageHelper->setParam("domain", filterCombo()->currentData().toString());
    else
        pageHelper->setParam("domain", "");
    pageHelper->setParam("sort", m_sortCol);
    pageHelper->setParam("order", m_sortOrder);
    pageHelper->loadPage(m_offset);
}

void TargetsFeedWidget::onFilterChanged()
{
    if (autoAction() && autoAction()->isChecked()) {
        m_offset = 0;
        loadCurrentPage();
    }
}

void TargetsFeedWidget::onSortingChanged(int index)
{
    switch (index) {
        case 0: m_sortCol = "Date";     m_sortOrder = "desc"; break;
        case 1: m_sortCol = "Date";     m_sortOrder = isSortAscending() ? "asc" : "desc"; break;
        case 2: m_sortCol = "Computer"; m_sortOrder = isSortAscending() ? "asc" : "desc"; break;
        case 3: m_sortCol = "Domain";   m_sortOrder = isSortAscending() ? "asc" : "desc"; break;
        case 4: m_sortCol = "Address";  m_sortOrder = isSortAscending() ? "asc" : "desc"; break;
        case 5: m_sortCol = "OsDesk";   m_sortOrder = isSortAscending() ? "asc" : "desc"; break;
        case 6: m_sortCol = "Tag";      m_sortOrder = isSortAscending() ? "asc" : "desc"; break;
        default: return;
    }
    m_offset = 0;
    loadCurrentPage();
}

void TargetsFeedWidget::onPageReady(const QJsonObject& response)
{
    QJsonArray items = response.value("items").toArray();
    QList<TargetData> newTargets;
    for (const auto& itemVal : items) {
        QJsonObject obj = itemVal.toObject();
        TargetData t;
        t.TargetId      = parseI64(obj, "t_target_id");
        t.Computer      = obj["t_computer"].toString();
        t.Domain        = obj["t_domain"].toString();
        t.Address       = obj["t_address"].toString();
        t.Tag           = obj["t_tag"].toString();
        t.Os            = obj["t_os"].toInt();
        t.OsDesc        = obj["t_os_desk"].toString();
        t.DateTimestamp = parseI64(obj, "t_date");
        t.Date          = UnixTimestampGlobalToStringLocal(t.DateTimestamp);
        t.Info          = obj["t_info"].toString();
        t.Alive         = obj["t_alive"].toBool();
        for (const QJsonValue& av : obj["t_agents"].toArray()) {
            if (av.isDouble() || av.isString())
                t.Agents.append(parseI64(av));
        }
        t.OsIcon = targetsOsIconFor(t.Os, !t.Agents.isEmpty(), t.Alive);
        newTargets.append(t);
    }

    {
        QWriteLocker locker(&m_adaptixWidget->TargetsLock);
        QSet<qint64> existingIds;
        for (const auto& et : m_adaptixWidget->Targets)
            existingIds.insert(et.TargetId);
        for (const auto& nt : newTargets) {
            if (!existingIds.contains(nt.TargetId)) {
                m_adaptixWidget->Targets.push_back(nt);
                existingIds.insert(nt.TargetId);
            }
        }
    }

    m_targetCache.clear();
    feedBlockModel->clear();
    QSet<QString> domains;
    bool hasEmptyDomain = false;
    for (const auto& t : newTargets) {
        m_targetCache[t.TargetId] = t;
        FeedRow row = targetToFeedRow(t);
        feedBlockModel->addRow(row);
        if (t.Domain.isEmpty())
            hasEmptyDomain = true;
        else
            domains.insert(t.Domain);
    }

    if (filterCombo()) {
        filterCombo()->blockSignals(true);
        QString current = filterCombo()->currentText();
        filterCombo()->clear();
        filterCombo()->addItem("All domains", "");
        if (hasEmptyDomain)
            filterCombo()->addItem("No domain", "__no_domain__");
        for (const auto& d : domains)
            filterCombo()->addItem(d, d);
        int idx = filterCombo()->findText(current);
        if (idx >= 0) filterCombo()->setCurrentIndex(idx);
        filterCombo()->blockSignals(false);
    }

    int total = response["total"].toInt();
    int shown = newTargets.size();
    int from  = shown == 0 ? 0 : m_offset + 1;
    int to    = m_offset + shown;
    paginationBar()->setInfo(from, to, total);
    paginationBar()->setPrevEnabled(m_offset > 0);
    paginationBar()->setNextEnabled(m_offset + shown < total);

    cachePrimed = true;
}

void TargetsFeedWidget::onPageError(const QString& message)
{
    Q_UNUSED(message);
    m_targetCache.clear();
    feedBlockModel->clear();
    paginationBar()->setInfo(0, 0, 0);
    paginationBar()->setPrevEnabled(false);
    paginationBar()->setNextEnabled(false);
    cachePrimed = false;
}

void TargetsFeedWidget::SetUpdatesEnabled(bool enabled)
{
    treeView()->setUpdatesEnabled(enabled);
    if (enabled && !cachePrimed)
        loadCurrentPage();
}

void TargetsFeedWidget::Clear()
{
    m_targetCache.clear();
    if (feedBlockModel)
        feedBlockModel->clear();

    m_offset = 0;
    cachePrimed = false;

    if (filterCombo()) {
        filterCombo()->blockSignals(true);
        filterCombo()->clear();
        filterCombo()->addItem("All domains", "");
        filterCombo()->blockSignals(false);
    }

    paginationBar()->setInfo(0, 0, 0);
    paginationBar()->setPrevEnabled(false);
    paginationBar()->setNextEnabled(false);
}

void TargetsFeedWidget::UpdateColumnsVisible()
{
    if (!feedBlockModel)
        return;
    for (int i = 0; i < feedBlockModel->size(); ++i) {
        const FeedRow& r = feedBlockModel->rowAt(i);
        auto it = m_targetCache.constFind(r.entityId);
        if (it == m_targetCache.constEnd())
            continue;
        feedBlockModel->updateRow(i, targetToFeedRow(it.value()));
    }
    auto* delegate = qobject_cast<ListFeedDelegate*>(treeView()->itemDelegate());
    if (delegate)
        delegate->updateMaxWidths(feedBlockModel);
}



void TargetsFeedWidget::AddTargetsItems(QList<TargetData> targetList)
{
    if (targetList.isEmpty())
        return;

    QList<TargetData> filtered;
    {
        QWriteLocker locker(&m_adaptixWidget->TargetsLock);
        QSet<qint64> existingIds;
        for (const auto& t : m_adaptixWidget->Targets)
            existingIds.insert(t.TargetId);

        for (const auto& target : targetList) {
            if (existingIds.contains(target.TargetId))
                continue;
            existingIds.insert(target.TargetId);
            m_adaptixWidget->Targets.push_back(target);
            filtered.append(target);
        }
    }

    if (filtered.isEmpty())
        return;

    if (cachePrimed) {
        m_offset = 0;
        loadCurrentPage();
    }
}

void TargetsFeedWidget::EditTargetsItem(const TargetData& newTarget)
{
    {
        QWriteLocker locker(&m_adaptixWidget->TargetsLock);
        for (int i = 0; i < m_adaptixWidget->Targets.size(); i++) {
            if (m_adaptixWidget->Targets[i].TargetId == newTarget.TargetId) {
                TargetData* td = &m_adaptixWidget->Targets[i];
                td->Computer    = newTarget.Computer;
                td->Domain      = newTarget.Domain;
                td->Address     = newTarget.Address;
                td->Tag         = newTarget.Tag;
                td->Os          = newTarget.Os;
                td->OsIcon      = newTarget.OsIcon;
                td->OsDesc      = newTarget.OsDesc;
                td->Date        = newTarget.Date;
                td->Info        = newTarget.Info;
                td->Alive       = newTarget.Alive;
                td->Agents      = newTarget.Agents;
                break;
            }
        }
    }

    m_targetCache[newTarget.TargetId] = newTarget;

    if (!cachePrimed)
        return;

    for (int i = 0; i < feedBlockModel->size(); ++i) {
        const FeedRow& r = feedBlockModel->rowAt(i);
        if (r.entityId == newTarget.TargetId) {
            FeedRow newRow = targetToFeedRow(newTarget);
            feedBlockModel->updateRow(i, newRow);
            return;
        }
    }
}

void TargetsFeedWidget::RemoveTargetsItem(const QList<qint64>& targetsId)
{
    {
        QWriteLocker locker(&m_adaptixWidget->TargetsLock);
        for (auto targetId : targetsId) {
            for (int i = 0; i < m_adaptixWidget->Targets.size(); i++) {
                if (m_adaptixWidget->Targets[i].TargetId == targetId) {
                    m_adaptixWidget->Targets.erase(m_adaptixWidget->Targets.begin() + i);
                    break;
                }
            }
        }
    }

    if (!cachePrimed)
        return;

    loadCurrentPage();
}

void TargetsFeedWidget::TargetsSetTag(const QList<qint64>& targetIds, const QString& tag)
{
    {
        QWriteLocker locker(&m_adaptixWidget->TargetsLock);
        QSet<qint64> idSet(targetIds.begin(), targetIds.end());
        for (int i = 0; i < m_adaptixWidget->Targets.size(); i++) {
            if (idSet.contains(m_adaptixWidget->Targets[i].TargetId)) {
                m_adaptixWidget->Targets[i].Tag = tag;
                idSet.remove(m_adaptixWidget->Targets[i].TargetId);
                if (idSet.isEmpty())
                    break;
            }
        }
    }

    if (!cachePrimed)
        return;

    loadCurrentPage();
}



void TargetsFeedWidget::TargetsAdd(QList<TargetData> targetList)
{
    QJsonArray jsonArray;
    for (const auto& target : targetList) {
        QJsonObject obj;
        obj["computer"] = target.Computer;
        obj["domain"]   = target.Domain;
        obj["address"]  = target.Address;
        obj["os"]       = target.Os;
        obj["os_desk"]  = target.OsDesc;
        obj["tag"]      = target.Tag;
        obj["info"]     = target.Info;
        obj["alive"]    = target.Alive;
        jsonArray.append(obj);
    }

    QJsonObject dataJson;
    dataJson["targets"] = jsonArray;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpReqTargetsCreateAsync(jsonData, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message);
    });
}



TargetsFeedWidget::TargetInfo TargetsFeedWidget::currentTargetInfo() const
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
    TargetInfo info;
    info.targetId = r.entityId;
    info.computer = r.blockData[TargetsBlock::Main].toMap()["computer"].toString();
    info.domain   = r.blockData[TargetsBlock::Main].toMap()["domain"].toString();
    info.address  = r.blockData[TargetsBlock::Text].toMap()["address"].toString();
    if (info.address.isEmpty())
        info.address = r.blockData[TargetsBlock::Text].toMap()["main"].toString();
    info.valid    = info.targetId > 0;
    return info;
}



void TargetsFeedWidget::onItemDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid())
        return;

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
    if (!srcIdx.isValid() || srcIdx.row() < 0 || srcIdx.row() >= feedBlockModel->size())
        return;

    const FeedRow& r = feedBlockModel->rowAt(srcIdx.row());
    if (r.entityId > 0)
        onEditTarget();
}

void TargetsFeedWidget::handleFeedMenu(const QPoint& pos)
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
            qint64 targetId = row.entityId;
            if (targetId > 0) {
                QStringList targetIds;
                QModelIndexList selectedRows = treeView()->selectionModel()->selectedRows();
                for (const QModelIndex& selIdx : selectedRows) {
                    qint64 selId = selIdx.data(Qt::UserRole).toLongLong();
                    if (selId > 0)
                        targetIds.append(QString::number(selId));
                }
                if (targetIds.isEmpty())
                    targetIds.append(QString::number(targetId));

                int topCount = m_adaptixWidget->ScriptManager->AddMenuTargets(&ctxMenu, "TargetsTop", targetIds);
                if (topCount > 0)
                    ctxMenu.addSeparator();

                ctxMenu.addAction("Edit",   this, &TargetsFeedWidget::onEditTarget);
                ctxMenu.addAction("Remove", this, &TargetsFeedWidget::onRemoveTarget);
                ctxMenu.addSeparator();

                int centerCount = m_adaptixWidget->ScriptManager->AddMenuTargets(&ctxMenu, "TargetsCenter", targetIds);
                if (centerCount > 0)
                    ctxMenu.addSeparator();

                ctxMenu.addAction("Set tag",           this, &TargetsFeedWidget::onSetTag);
                ctxMenu.addAction("Export to file",    this, &TargetsFeedWidget::onExportTarget);
                ctxMenu.addAction("Copy to clipboard", this, &TargetsFeedWidget::onCopyToClipboard);
                m_adaptixWidget->ScriptManager->AddMenuTargets(&ctxMenu, "TargetsBottom", targetIds);
            }
        }
    }

    ctxMenu.exec(treeView()->viewport()->mapToGlobal(pos));
}

void TargetsFeedWidget::onCreateTarget()
{
    if (!m_adaptixWidget)
        return;

    DialogTarget* dialogTargets = new DialogTarget();
    while (true) {
        dialogTargets->StartDialog();
        if (dialogTargets->IsValid())
            break;

        QString msg = dialogTargets->GetMessage();
        if (msg.isEmpty()) {
            delete dialogTargets;
            return;
        }

        MessageError(msg);
    }

    TargetData targetData = dialogTargets->GetTargetData();
    delete dialogTargets;

    QList<TargetData> targetList;
    targetList.append(targetData);
    this->TargetsAdd(targetList);
}

void TargetsFeedWidget::onEditTarget()
{
    TargetInfo info = currentTargetInfo();
    if (!info.valid)
        return;

    TargetData targetData;
    bool found = false;
    {
        QReadLocker locker(&m_adaptixWidget->TargetsLock);
        for (const auto& target : m_adaptixWidget->Targets) {
            if (target.TargetId == info.targetId) {
                targetData = target;
                found = true;
                break;
            }
        }
    }
    if (!found)
        return;

    DialogTarget* dialogTarget = new DialogTarget();
    dialogTarget->SetEditmode(targetData);
    while (true) {
        dialogTarget->StartDialog();
        if (dialogTarget->IsValid())
            break;

        QString msg = dialogTarget->GetMessage();
        if (msg.isEmpty()) {
            delete dialogTarget;
            return;
        }

        MessageError(msg);
    }

    TargetData newTargetData = dialogTarget->GetTargetData();

    QJsonObject dataJson;
    dataJson["t_target_id"] = static_cast<qint64>(newTargetData.TargetId);
    dataJson["t_computer"]  = newTargetData.Computer;
    dataJson["t_domain"]    = newTargetData.Domain;
    dataJson["t_address"]   = newTargetData.Address;
    dataJson["t_os"]        = newTargetData.Os;
    dataJson["t_os_desk"]   = newTargetData.OsDesc;
    dataJson["t_tag"]       = newTargetData.Tag;
    dataJson["t_info"]      = newTargetData.Info;
    dataJson["t_alive"]     = newTargetData.Alive;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    delete dialogTarget;

    HttpReqTargetEditAsync(jsonData, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Server is not responding" : message);
    });
}

void TargetsFeedWidget::onRemoveTarget()
{
    TargetInfo info = currentTargetInfo();
    if (!info.valid)
        return;

    QList<qint64> listId;
    QModelIndexList selectedRows = treeView()->selectionModel()->selectedRows();
    for (const QModelIndex& idx : selectedRows) {
        qint64 id = idx.data(Qt::UserRole).toLongLong();
        if (id > 0)
            listId.append(id);
    }
    if (listId.isEmpty())
        listId.append(info.targetId);

    HttpReqTargetRemoveAsync(listId, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void TargetsFeedWidget::onSetTag()
{
    TargetInfo info = currentTargetInfo();
    if (!info.valid)
        return;

    QString currentTag;
    QList<qint64> listId;
    QModelIndexList selectedRows = treeView()->selectionModel()->selectedRows();
    for (const QModelIndex& idx : selectedRows) {
        qint64 id = idx.data(Qt::UserRole).toLongLong();
        if (id > 0) {
            listId.append(id);
            if (currentTag.isEmpty()) {
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
                if (srcIdx.isValid() && srcIdx.row() >= 0 && srcIdx.row() < feedBlockModel->size()) {
                    const FeedRow& r = feedBlockModel->rowAt(srcIdx.row());
                    currentTag = r.blockData[TargetsBlock::Tags].toStringList().join(",");
                }
            }
        }
    }
    if (listId.isEmpty())
        return;

    bool inputOk;
    QString newTag = QInputDialog::getText(nullptr, "Set tags", "New tag", QLineEdit::Normal, currentTag, &inputOk);
    if (inputOk) {
        HttpReqTargetSetTagAsync(listId, newTag, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void TargetsFeedWidget::onExportTarget()
{
    QModelIndexList selectedRows = treeView()->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
        return;

    QInputDialog dialog;
    dialog.setWindowTitle("Format for saving");
    dialog.setLabelText("Format:");
    dialog.setTextValue("%computer%.%domain% - %address%");
    QLineEdit* lineEdit = dialog.findChild<QLineEdit*>();
    if (lineEdit)
        lineEdit->setMinimumWidth(400);

    bool inputOk = (dialog.exec() == QDialog::Accepted);
    if (!inputOk)
        return;

    QString format = dialog.textValue();

    QString baseDir = QStringLiteral("targets.txt");
    if (m_adaptixWidget && m_adaptixWidget->GetProfile())
        baseDir = QDir(m_adaptixWidget->GetProfile()->GetProjectDir()).filePath(QStringLiteral("targets.txt"));

    NonBlockingDialogs::getSaveFileName(this, "Save Targets", baseDir, "Text Files (*.txt);;All Files (*)",
        [this, format, selectedRows](const QString& fileName) {
            if (fileName.isEmpty())
                return;

            QFile file(fileName);
            if (!file.open(QIODevice::WriteOnly)) {
                MessageError("Failed to open file for writing");
                return;
            }

            QString content;
            for (const QModelIndex& idx : selectedRows) {
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
                    continue;

                const FeedRow& r = feedBlockModel->rowAt(srcIdx.row());
                QString computer = r.blockData[TargetsBlock::Main].toMap()["computer"].toString();
                QString domain   = r.blockData[TargetsBlock::Main].toMap()["domain"].toString();
                QString address  = r.blockData[TargetsBlock::Text].toMap()["main"].toString();
                if (address.isEmpty())
                    address = r.blockData[TargetsBlock::Text].toMap()["address"].toString();

                QString temp = format;
                content += temp
                    .replace("%computer%", computer)
                    .replace("%domain%", domain)
                    .replace("%address%", address)
                    + "\n";
            }

            file.write(content.trimmed().toUtf8());
            file.close();
    });
}

void TargetsFeedWidget::onCopyToClipboard()
{
    QModelIndexList selectedRows = treeView()->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
        return;

    QInputDialog dialog;
    dialog.setWindowTitle("Format for clipboard");
    dialog.setLabelText("Format:");
    dialog.setTextValue("%computer%.%domain% - %address%");
    QLineEdit* lineEdit = dialog.findChild<QLineEdit*>();
    if (lineEdit)
        lineEdit->setMinimumWidth(400);

    bool inputOk = (dialog.exec() == QDialog::Accepted);
    if (!inputOk)
        return;

    QString format = dialog.textValue();

    QString content;
    for (const QModelIndex& idx : selectedRows) {
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
            continue;

        const FeedRow& r = feedBlockModel->rowAt(srcIdx.row());
        QString computer = r.blockData[TargetsBlock::Main].toMap()["computer"].toString();
        QString domain   = r.blockData[TargetsBlock::Main].toMap()["domain"].toString();
        QString address  = r.blockData[TargetsBlock::Text].toMap()["main"].toString();
        if (address.isEmpty())
            address = r.blockData[TargetsBlock::Text].toMap()["address"].toString();

        QString temp = format;
        content += temp
            .replace("%computer%", computer)
            .replace("%domain%", domain)
            .replace("%address%", address)
            + "\n";
    }

    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(content.trimmed());
}
