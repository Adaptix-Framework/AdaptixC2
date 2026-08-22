#include <UI/Widgets/TargetsWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <UI/Dialogs/DialogTarget.h>
#include <UI/Dialogs/DialogImportTargets.h>
#include <Utils/Logs.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/Settings.h>
#include <Client/AxScript/AxScriptManager.h>
#include <MainAdaptix.h>
#include <Utils/CustomElements/BoldHeaderView.h>
#include <Utils/CustomElements/ClickableLabel.h>
#include <Utils/CustomElements/Delegates.h>
#include <Utils/CustomElements/PageNavBar.h>
#include <Utils/CustomElements/SearchPanel.h>
#include <Utils/NonBlockingDialogs.h>

#include <oclero/qlementine/widgets/Menu.hpp>
#include <oclero/qlementine/utils/ImageUtils.hpp>

REGISTER_DOCK_WIDGET(TargetsWidget, "Targets", true)



QVariant TargetsTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= targets.size())
        return {};

    const TargetData& t = targets.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case TRC_Id:       return QString("#%1").arg(t.TargetId);
            case TRC_Computer: return t.Computer;
            case TRC_Domain:   return t.Domain;
            case TRC_Address:  return t.Address;
            case TRC_Tag:      return t.Tag;
            case TRC_Os:       return t.OsDesc;
            case TRC_Date:     return t.Date;
            case TRC_Info:     return t.Info;
            case TRC_Status:   return t.Alive ? QStringLiteral("Alive") : QStringLiteral("Offline");
            default: ;
        }
    }

    if (role == Qt::UserRole) {
        switch (index.column()) {
            case TRC_Id:     return t.TargetId;
            case TRC_Date:   return t.DateTimestamp;
            case TRC_Status: return t.Alive;
            default:         return data(index, Qt::DisplayRole);
        }
    }

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
            case TRC_Address:
            case TRC_Date:
            case TRC_Status:
                return Qt::AlignCenter;
            default: ;
        }
    }

    if (role == Qt::DecorationRole && index.column() == TRC_Os) {
        const bool showIcon = GlobalClient && GlobalClient->settings && GlobalClient->settings->data.TargetsTableColumns[0]; // TF::Icon
        if (showIcon)
            return t.OsIcon;
    }

    return {};
}

QVariant TargetsTableModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (role != Qt::DisplayRole || o != Qt::Horizontal)
        return {};

    static QStringList headers = { "ID","Computer","Domain","Address","Tag", "OS","Date","Info","Status" };
    return headers.value(section);
}

void TargetsTableModel::add(const TargetData& item)
{
    const int row = targets.size();
    beginInsertRows({}, row, row);
    targets.append(item);
    idToRow[item.TargetId] = row;
    endInsertRows();
}

void TargetsTableModel::add(const QList<TargetData>& list)
{
    if (list.isEmpty())
        return;
    const int start = targets.size();
    const int end   = start + list.size() - 1;
    beginInsertRows({}, start, end);
    for (const auto& item : list) {
        idToRow[item.TargetId] = targets.size();
        targets.append(item);
    }
    endInsertRows();
}

void TargetsTableModel::update(qint64 targetId, const TargetData& newTarget)
{
    auto it = idToRow.find(targetId);
    if (it == idToRow.end())
        return;
    int row = it.value();
    targets[row] = newTarget;
    Q_EMIT dataChanged(index(row, 0), index(row, TRC_ColumnCount - 1));
}

void TargetsTableModel::remove(const QList<qint64>& targetIds)
{
    if (targetIds.isEmpty() || targets.isEmpty())
        return;

    QList<int> rowsToRemove;
    rowsToRemove.reserve(targetIds.size());
    for (qint64 id : targetIds) {
        auto it = idToRow.find(id);
        if (it != idToRow.end())
            rowsToRemove.append(it.value());
    }
    if (rowsToRemove.isEmpty())
        return;

    std::ranges::sort(rowsToRemove, std::greater<int>());

    for (int row : rowsToRemove) {
        beginRemoveRows({}, row, row);
        idToRow.remove(targets[row].TargetId);
        targets.removeAt(row);
        endRemoveRows();
    }
    rebuildIndex();
}

void TargetsTableModel::setTag(const QList<qint64>& targetIds, const QString& tag)
{
    if (targetIds.isEmpty() || targets.isEmpty())
        return;

    for (qint64 id : targetIds) {
        auto it = idToRow.find(id);
        if (it == idToRow.end())
            continue;
        int row = it.value();
        targets[row].Tag = tag;
        Q_EMIT dataChanged(index(row, TRC_Tag), index(row, TRC_Tag), {Qt::DisplayRole});
    }
}

void TargetsTableModel::clear()
{
    beginResetModel();
    targets.clear();
    idToRow.clear();
    endResetModel();
}

void TargetsTableModel::reset(const QList<TargetData>& newTargets)
{
    beginResetModel();
    targets.clear();
    idToRow.clear();
    for (const auto& t : newTargets) {
        idToRow[t.TargetId] = targets.size();
        targets.append(t);
    }
    endResetModel();
}



TargetsWidget::TargetsWidget(AdaptixWidget* w) : DockTab("Targets", w->GetProfile()->GetProject(), ":/icons/devices", w), adaptixWidget(w)
{
    this->createUI();

    pageHelper = new PagedTableHelper(w->GetProfile(), "/targets/list", this);
    pageHelper->setPageSize(pageNavBar->pageSize());

    connect(pageHelper, &PagedTableHelper::pageReady,      this, &TargetsWidget::onPageReady);
    connect(pageHelper, &PagedTableHelper::errorOccurred,  this, &TargetsWidget::onPageError);
    connect(pageHelper, &PagedTableHelper::loadingChanged, this, [this](bool loading) {
        pageNavBar->setLoading(loading);
        tableView->setEnabled(!loading);
    });

    connect(pageNavBar, &PageNavBar::prevClicked, this, [this]() {
        m_offset = qMax(0, m_offset - pageNavBar->pageSize());
        loadCurrentPage();
    });
    connect(pageNavBar, &PageNavBar::nextClicked, this, [this]() {
        m_offset += pageNavBar->pageSize();
        loadCurrentPage();
    });
    connect(pageNavBar, &PageNavBar::pageSizeChanged, this, [this](int size) {
        pageHelper->setPageSize(size);
        m_offset = 0;
        loadCurrentPage();
    });
    connect(pageNavBar, &PageNavBar::filterChanged, this, [this]() {
        m_offset = 0;
        loadCurrentPage();
    });

    connect(tableView, &QTableView::customContextMenuRequested, this, &TargetsWidget::handleTargetsMenu);
    connect(tableView->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu;
        menu.addAction("Resize columns", this, &TargetsWidget::UpdateColumnsSize);
        menu.exec(tableView->horizontalHeader()->viewport()->mapToGlobal(pos));
    });
    connect(tableView, &QTableView::doubleClicked, this, &TargetsWidget::onEditTarget);
    connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection&, const QItemSelection&) {
            tableView->setFocus();
        });
    connect(tableView->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this](int section, Qt::SortOrder order) {
        QString key = sortKeyForTargetSection(section);
        if (key.isEmpty())
            return;

        QString newCol   = key;
        QString newOrder = (order == Qt::AscendingOrder) ? "asc" : "desc";
        if (newCol == m_sortCol && newOrder == m_sortOrder)
            return;

        m_sortCol = newCol;
        m_sortOrder = newOrder;
        m_offset = 0;
        loadCurrentPage();
    });

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), this);
    shortcutSearch->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, [this]() { pageNavBar->focusFilter(); });

    this->dockWidget->setWidget(this);
}

TargetsWidget::~TargetsWidget() = default;

void TargetsWidget::createUI()
{
    targetsModel = new TargetsTableModel(this);
    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(targetsModel);
    proxyModel->setSortRole(Qt::UserRole);

    tableView = new QTableView( this );
    tableView->setModel(proxyModel);
    tableView->setHorizontalHeader(new BoldHeaderView(Qt::Horizontal, tableView));
    tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    tableView->setAutoFillBackground(false);
    tableView->setShowGrid(false);
    tableView->setSortingEnabled(true);
    tableView->setWordWrap(true);
    tableView->setCornerButtonEnabled(false);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setFocusPolicy(Qt::ClickFocus);
    tableView->setAlternatingRowColors(true);
    tableView->setProperty("autoIconColor", QVariant::fromValue(oclero::qlementine::AutoIconColor::None));
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    tableView->horizontalHeader()->setCascadingSectionResizes(true);
    tableView->horizontalHeader()->setHighlightSections(false);
    tableView->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    tableView->horizontalHeader()->setSortIndicatorShown(true);
    tableView->horizontalHeader()->setSectionsClickable(true);
    tableView->sortByColumn(TRC_Date, Qt::DescendingOrder);
    tableView->verticalHeader()->setVisible(false);

    tableView->horizontalHeader()->setSectionResizeMode(TRC_Computer, QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(TRC_Domain,   QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(TRC_Address,  QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(TRC_Date,     QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(TRC_Tag,      QHeaderView::Stretch);
    tableView->horizontalHeader()->setSectionResizeMode(TRC_Os,       QHeaderView::Stretch);
    tableView->horizontalHeader()->setSectionResizeMode(TRC_Info,     QHeaderView::Stretch);

    tableView->setItemDelegate(new PaddingDelegate(tableView));

    this->UpdateColumnsVisible();

    pageNavBar = new PageNavBar(this);
    pageNavBar->setFilterPlaceholder("filter: (win | linux) & ^(test)");
    pageNavBar->setAgentComboVisible(false);

    mainGridLayout = new QGridLayout(this);
    mainGridLayout->setContentsMargins(0, 0, 0, 0);
    mainGridLayout->setVerticalSpacing(4);
    mainGridLayout->setHorizontalSpacing(8);
    mainGridLayout->addWidget(pageNavBar, 0, 0, 1, 1);
    mainGridLayout->addWidget(tableView,  1, 0, 1, 1);
    setLayout(mainGridLayout);
}


void TargetsWidget::loadCurrentPage()
{
    pageHelper->setParam("q",     pageNavBar->filterText());
    pageHelper->setParam("sort",  m_sortCol);
    pageHelper->setParam("order", m_sortOrder);
    pageHelper->loadPage(m_offset);
}

static QIcon osIconFor(int os, bool owned, bool alive)
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

void TargetsWidget::onPageReady(const QJsonObject& response)
{
    QJsonArray items = response.value("items").toArray();

    QList<TargetData> page;
    page.reserve(items.size());

    for (const QJsonValue& v : items) {
        QJsonObject obj = v.toObject();
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
        t.OsIcon = osIconFor(t.Os, !t.Agents.isEmpty(), t.Alive);
        page.append(t);
    }

    targetsModel->reset(page);
    proxyModel->invalidate();

    int total = response["total"].toInt();
    int shown = page.size();
    int from  = shown == 0 ? 0 : m_offset + 1;
    int to    = m_offset + shown;
    pageNavBar->setInfo(from, to, total);
    pageNavBar->setPrevEnabled(m_offset > 0);
    pageNavBar->setNextEnabled(m_offset + shown < total);

    cachePrimed = true;
}

void TargetsWidget::onPageError(const QString& message)
{
    if (message.contains("invalid filter", Qt::CaseInsensitive) || message.startsWith("filter:", Qt::CaseInsensitive))
        return;

    targetsModel->clear();
    proxyModel->invalidate();
    pageNavBar->setError(message);
    pageNavBar->setPrevEnabled(false);
    pageNavBar->setNextEnabled(false);
    cachePrimed = false;
}

void TargetsWidget::SetUpdatesEnabled(const bool enabled)
{
    bufferingEnabled = !enabled;
    tableView->setUpdatesEnabled(enabled);
    if (enabled && !cachePrimed)
        loadCurrentPage();
}

/// Main

void TargetsWidget::AddTargetsItems(QList<TargetData> targetList)
{
    if (targetList.isEmpty())
        return;

    QList<TargetData> filtered;
    {
        QWriteLocker locker(&adaptixWidget->TargetsLock);
        QSet<qint64> existingIds;
        for (const auto& t : adaptixWidget->Targets)
            existingIds.insert(t.TargetId);

        for (const auto& target : targetList) {
            if (existingIds.contains(target.TargetId))
                continue;
            existingIds.insert(target.TargetId);
            adaptixWidget->Targets.push_back(target);
            filtered.append(target);
        }
    }

    if (filtered.isEmpty())
        return;

    if (bufferingEnabled)
        return;

    if (cachePrimed)
        loadCurrentPage();
}

void TargetsWidget::EditTargetsItem(const TargetData &newTarget)
{
    {
        QWriteLocker locker(&adaptixWidget->TargetsLock);
        for ( int i = 0; i < adaptixWidget->Targets.size(); i++ ) {
            if( adaptixWidget->Targets[i].TargetId == newTarget.TargetId ) {
                TargetData* td = &adaptixWidget->Targets[i];
                td->Computer = newTarget.Computer;
                td->Domain   = newTarget.Domain;
                td->Address  = newTarget.Address;
                td->Tag      = newTarget.Tag;
                td->Os       = newTarget.Os;
                td->OsIcon   = newTarget.OsIcon;
                td->OsDesc   = newTarget.OsDesc;
                td->Date     = newTarget.Date;
                td->Info     = newTarget.Info;
                td->Alive    = newTarget.Alive;
                td->Agents   = newTarget.Agents;
                break;
            }
        }
    }

    if (bufferingEnabled)
        return;

    if (!cachePrimed)
        return;

    if (targetsModel->containsId(newTarget.TargetId)) {
        targetsModel->update(newTarget.TargetId, newTarget);
    }
    else {
        loadCurrentPage();
    }
}

void TargetsWidget::RemoveTargetsItem(const QList<qint64> &targetsId)
{
    QList<qint64> filtered;
    {
        QWriteLocker locker(&adaptixWidget->TargetsLock);
        for (auto targetId : targetsId) {
            for ( int i = 0; i < adaptixWidget->Targets.size(); i++ ) {
                if( adaptixWidget->Targets[i].TargetId == targetId ) {
                    filtered.append(targetId);
                    adaptixWidget->Targets.erase( adaptixWidget->Targets.begin() + i );
                    break;
                }
            }
        }
    }

    if (bufferingEnabled)
        return;

    if (cachePrimed)
        loadCurrentPage();
}

void TargetsWidget::TargetsSetTag(const QList<qint64> &targetIds, const QString &tag)
{
    {
        QWriteLocker locker(&adaptixWidget->TargetsLock);
        QSet<qint64> set1 = QSet<qint64>(targetIds.begin(), targetIds.end());
        for ( int i = 0; i < adaptixWidget->Targets.size(); i++ ) {
            if( set1.contains(adaptixWidget->Targets[i].TargetId) ) {
                adaptixWidget->Targets[i].Tag = tag;
                set1.remove(adaptixWidget->Targets[i].TargetId);
                if (set1.size() == 0)
                    break;
            }
        }
    }

    if (bufferingEnabled)
        return;

    targetsModel->setTag(targetIds, tag);
}

void TargetsWidget::UpdateColumnsSize() const
{
    for (int i = 0; i < TRC_ColumnCount; ++i) {
        if (!tableView->isColumnHidden(i) && tableView->horizontalHeader()->sectionResizeMode(i) != QHeaderView::Stretch) {
            tableView->resizeColumnToContents(i);
        }
    }
}

void TargetsWidget::UpdateColumnsVisible()
{
    if (!tableView || !GlobalClient || !GlobalClient->settings)
        return;
    const auto& cols = GlobalClient->settings->data.TargetsTableColumns;
    const int map[TRC_ColumnCount] = {
        1, // TRC_Id
        3, // TRC_Computer
        4, // TRC_Domain
        5, // TRC_Address
        8, // TRC_Tag
        6, // TRC_Os
        2, // TRC_Date
        7, // TRC_Info
        9, // TRC_Status
    };
    for (int i = 0; i < TRC_ColumnCount; ++i) {
        if (cols[map[i]])
            tableView->showColumn(i);
        else
            tableView->hideColumn(i);
    }
}


void TargetsWidget::Clear()
{
    {
        QWriteLocker locker(&adaptixWidget->TargetsLock);
        adaptixWidget->Targets.clear();
    }

    m_offset = 0;
    cachePrimed = false;
    targetsModel->clear();

    pageNavBar->blockSignals(true);
    pageNavBar->clearFilter();
    pageNavBar->blockSignals(false);

    pageNavBar->setInfo(0, 0, 0);
    pageNavBar->setPrevEnabled(false);
    pageNavBar->setNextEnabled(false);
}

/// Sender

void TargetsWidget::TargetsAdd(QList<TargetData> targetList)
{
    QJsonArray jsonArray;
    for (const auto &target : targetList) {
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

    HttpReqTargetsCreateAsync(jsonData, *(adaptixWidget->GetProfile()), [](bool success, const QString &message, const QJsonObject&) {
        if (!success)
            MessageError(message);
    });
}

/// Slots

void TargetsWidget::handleTargetsMenu(const QPoint &pos ) const
{
    oclero::qlementine::Menu ctxMenu;
    ctxMenu.addAction(QIcon(":/icons/plus"), "Create", this, &TargetsWidget::onCreateTarget );
    ctxMenu.addAction(QIcon(":/icons/file_open"), "Import...", this, &TargetsWidget::onImportTargets );

    QModelIndex index = tableView->indexAt(pos);
    if (index.isValid()) {

        QStringList targets;
        QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
        for (const QModelIndex &proxyIndex : selectedRows) {
            QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
            if (!sourceIndex.isValid())
                continue;

            qint64 id = targetsModel->data(targetsModel->index(sourceIndex.row(), TRC_Id), Qt::UserRole).toLongLong();
            targets.append(QString::number(id));
        }

        int topCount = adaptixWidget->ScriptManager->AddMenuTargets(&ctxMenu, "TargetsTop", targets);
        if (topCount > 0)
            ctxMenu.addSeparator();

        ctxMenu.addAction(QIcon(":/icons/edit_note"), "Edit", this, &TargetsWidget::onEditTarget );
        ctxMenu.addAction(QIcon(":/icons/delete"), "Remove", this, &TargetsWidget::onRemoveTarget );
        ctxMenu.addSeparator();

        int centerCount = adaptixWidget->ScriptManager->AddMenuTargets(&ctxMenu, "TargetsCenter", targets);
        if (centerCount > 0)
            ctxMenu.addSeparator();

        ctxMenu.addAction(QIcon(":/icons/tag"), "Set tag", this, &TargetsWidget::onSetTag );
        ctxMenu.addAction(QIcon(":/icons/save_as"), "Export to file", this, &TargetsWidget::onExportTarget );
        ctxMenu.addAction(QIcon(":/icons/copy_all"), "Copy to clipboard", this, &TargetsWidget::onCopyToClipboard );
        adaptixWidget->ScriptManager->AddMenuTargets(&ctxMenu, "TargetsBottom", targets);
    }
    QPoint globalPos = tableView->mapToGlobal(pos);
    ctxMenu.exec(globalPos);
}

void TargetsWidget::onImportTargets()
{
    DialogImportTargets dialog(this);
    dialog.StartDialog();
    if (!dialog.IsValid())
        return;

    const QList<TargetData> list = dialog.GetTargets();
    if (list.isEmpty())
        return;

    TargetsAdd(list);
    MessageSuccess(QStringLiteral("Importing %1 target(s)…").arg(list.size()));
}

void TargetsWidget::onCreateTarget()
{
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

void TargetsWidget::onEditTarget()
{
    auto idx = tableView->currentIndex();
    if (!idx.isValid())
        return;

    qint64 targetId = proxyModel->index(idx.row(), TRC_Id).data(Qt::UserRole).toLongLong();

    bool found = false;
    TargetData targetData;
    {
        QReadLocker locker(&adaptixWidget->TargetsLock);
        for (auto target : adaptixWidget->Targets) {
            if (target.TargetId == targetId) {
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

    HttpReqTargetEditAsync(jsonData, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Server is not responding" : message);
    });
}

void TargetsWidget::onRemoveTarget() const
{
    QList<qint64> listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid())
            continue;

        qint64 id = targetsModel->data(targetsModel->index(sourceIndex.row(), TRC_Id), Qt::UserRole).toLongLong();
        listId.append(id);
    }

    if(listId.empty())
        return;

    HttpReqTargetRemoveAsync(listId, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void TargetsWidget::onSetTag() const
{
    QString tag = "";
    QList<qint64> listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid())
            continue;

        QString cTag = targetsModel->data(targetsModel->index(sourceIndex.row(), TRC_Tag), Qt::DisplayRole).toString();
        qint64 id    = targetsModel->data(targetsModel->index(sourceIndex.row(), TRC_Id),  Qt::UserRole).toLongLong();
        listId.append(id);

        if (tag.isEmpty())
            tag = cTag;
    }

    if(listId.empty())
        return;

    bool inputOk;
    QString newTag = QInputDialog::getText(nullptr, "Set tags", "New tag", QLineEdit::Normal,tag, &inputOk);
    if ( inputOk ) {
        HttpReqTargetSetTagAsync(listId, newTag, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void TargetsWidget::onExportTarget() const
{
    auto idx = tableView->currentIndex();
    if (!idx.isValid())
        return;

    QInputDialog dialog;
    dialog.setWindowTitle("Format for saving");
    dialog.setLabelText("Format:");
    dialog.setTextValue("%computer%.%domain% - %address%");
    QLineEdit *lineEdit = dialog.findChild<QLineEdit*>();
    if (lineEdit)
        lineEdit->setMinimumWidth(400);

    bool inputOk = (dialog.exec() == QDialog::Accepted);
    if (!inputOk)
        return;

    QString format = dialog.textValue();

    QString baseDir = QStringLiteral("targets.txt");
    if (adaptixWidget && adaptixWidget->GetProfile())
        baseDir = QDir(adaptixWidget->GetProfile()->GetProjectDir()).filePath(QStringLiteral("targets.txt"));

    NonBlockingDialogs::getSaveFileName(const_cast<TargetsWidget*>(this), "Save Targets", baseDir, "Text Files (*.txt);;All Files (*)",
        [this, format](const QString& fileName) {
            if (fileName.isEmpty())
                return;

            QFile file(fileName);
            if (!file.open(QIODevice::WriteOnly)) {
                MessageError("Failed to open file for writing");
                return;
            }

            QString content = "";
            QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
            for (const QModelIndex &proxyIndex : selectedRows) {
                QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
                if (!sourceIndex.isValid())
                    continue;

                QString computer = targetsModel->data(targetsModel->index(sourceIndex.row(), TRC_Computer), Qt::DisplayRole).toString();
                QString domain   = targetsModel->data(targetsModel->index(sourceIndex.row(), TRC_Domain), Qt::DisplayRole).toString();
                QString address  = targetsModel->data(targetsModel->index(sourceIndex.row(), TRC_Address), Qt::DisplayRole).toString();

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

void TargetsWidget::onCopyToClipboard() const
{
    auto idx = tableView->currentIndex();
    if (!idx.isValid())
        return;

    QInputDialog dialog;
    dialog.setWindowTitle("Format for clipboard");
    dialog.setLabelText("Format:");
    dialog.setTextValue("%computer%.%domain% - %address%");
    QLineEdit *lineEdit = dialog.findChild<QLineEdit*>();
    if (lineEdit)
        lineEdit->setMinimumWidth(400);

    bool inputOk = (dialog.exec() == QDialog::Accepted);
    if (!inputOk)
        return;

    QString format = dialog.textValue();

    QString content = "";
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid())
            continue;

        QString computer = targetsModel->data(targetsModel->index(sourceIndex.row(), TRC_Computer), Qt::DisplayRole).toString();
        QString domain   = targetsModel->data(targetsModel->index(sourceIndex.row(), TRC_Domain), Qt::DisplayRole).toString();
        QString address  = targetsModel->data(targetsModel->index(sourceIndex.row(), TRC_Address), Qt::DisplayRole).toString();

        QString temp = format;
        content += temp
        .replace("%computer%", computer)
        .replace("%domain%", domain)
        .replace("%address%", address)
        + "\n";
    }

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(content.trimmed());
}
