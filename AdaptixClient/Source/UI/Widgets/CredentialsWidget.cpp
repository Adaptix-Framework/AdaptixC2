#include <Agent/Agent.h>
#include <UI/Widgets/CredentialsWidget.h>
#include <oclero/qlementine/widgets/Menu.hpp>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <UI/Dialogs/DialogCredential.h>
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

REGISTER_DOCK_WIDGET(CredentialsWidget, "Credentials", true)



QVariant CredsTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= creds.size())
        return {};

    const CredentialData& c = creds.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case CC_Id:       return QString("#%1").arg(c.CredId);
            case CC_Username: return c.Username;
            case CC_Password: return c.Password;
            case CC_Realm:    return c.Realm;
            case CC_Type:     return c.Type;
            case CC_Tag:      return c.Tag;
            case CC_Date:     return c.Date;
            case CC_Storage:  return c.Storage;
            case CC_Agent:    return c.AgentId == 0 ? QString() : QString("#%1").arg(c.AgentId);
            case CC_Host:     return c.Host;
            default: ;
        }
    }

    if (role == Qt::UserRole) {
        switch (index.column()) {
            case CC_Id:   return c.CredId;
            case CC_Date: return c.DateTimestamp;
            default:      return data(index, Qt::DisplayRole);
        }
    }

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
            case CC_Type:
            case CC_Date:
            case CC_Storage:
            case CC_Agent:
                return Qt::AlignCenter;
            default: ;
        }
    }

    return {};
}

QVariant CredsTableModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (role != Qt::DisplayRole || o != Qt::Horizontal)
        return {};

    static QStringList headers = { "ID","Username","Password","Realm","Type", "Tag","Date","Storage","Agent","Host" };
    return headers.value(section);
}

void CredsTableModel::add(const CredentialData& item)
{
    const int row = creds.size();
    beginInsertRows({}, row, row);
    creds.append(item);
    idToRow[item.CredId] = row;
    endInsertRows();
}

void CredsTableModel::add(const QList<CredentialData>& list)
{
    if (list.isEmpty())
        return;
    const int start = creds.size();
    const int end   = start + list.size() - 1;
    beginInsertRows({}, start, end);
    for (const auto& item : list) {
        idToRow[item.CredId] = creds.size();
        creds.append(item);
    }
    endInsertRows();
}

void CredsTableModel::update(qint64 credId, const CredentialData& newCred)
{
    auto it = idToRow.find(credId);
    if (it == idToRow.end())
        return;

    int row = it.value();
    creds[row] = newCred;
    Q_EMIT dataChanged(index(row, 0), index(row, CC_ColumnCount - 1));
}

void CredsTableModel::remove(const QList<qint64>& credIds)
{
    if (credIds.isEmpty() || creds.isEmpty())
        return;

    QList<int> rowsToRemove;
    rowsToRemove.reserve(credIds.size());
    for (qint64 id : credIds) {
        auto it = idToRow.find(id);
        if (it != idToRow.end())
            rowsToRemove.append(it.value());
    }
    if (rowsToRemove.isEmpty())
        return;

    std::ranges::sort(rowsToRemove, std::greater<int>());

    for (int row : rowsToRemove) {
        beginRemoveRows({}, row, row);
        idToRow.remove(creds[row].CredId);
        creds.removeAt(row);
        endRemoveRows();
    }
    rebuildIndex();
}

void CredsTableModel::setTag(const QList<qint64>& credIds, const QString& tag)
{
    if (credIds.isEmpty() || creds.isEmpty())
        return;

    for (qint64 id : credIds) {
        auto it = idToRow.find(id);
        if (it == idToRow.end())
            continue;
        int row = it.value();
        creds[row].Tag = tag;
        Q_EMIT dataChanged(index(row, CC_Tag), index(row, CC_Tag), {Qt::DisplayRole});
    }
}

void CredsTableModel::clear()
{
    beginResetModel();
    creds.clear();
    idToRow.clear();
    endResetModel();
}

void CredsTableModel::reset(const QList<CredentialData>& newCreds)
{
    beginResetModel();
    creds.clear();
    idToRow.clear();
    for (const auto& c : newCreds) {
        idToRow[c.CredId] = creds.size();
        creds.append(c);
    }
    endResetModel();
}



CredentialsWidget::CredentialsWidget(AdaptixWidget* w) : DockTab("Credentials", w->GetProfile()->GetProject(), ":/icons/key"), adaptixWidget(w)
{
    this->createUI();

    pageHelper = new PagedTableHelper(w->GetProfile(), "/creds/list", this);
    pageHelper->setPageSize(pageNavBar->pageSize());

    connect(pageHelper, &PagedTableHelper::pageReady,      this, &CredentialsWidget::onPageReady);
    connect(pageHelper, &PagedTableHelper::errorOccurred,  this, &CredentialsWidget::onPageError);
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
    connect(pageNavBar, &PageNavBar::agentChanged, this, [this]() {
        m_offset = 0;
        loadCurrentPage();
    });

    connect(tableView, &QTableView::customContextMenuRequested, this, &CredentialsWidget::handleCredentialsMenu);
    connect(tableView->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu;
        menu.addAction("Resize columns", this, &CredentialsWidget::UpdateColumnsSize);
        menu.exec(tableView->horizontalHeader()->viewport()->mapToGlobal(pos));
    });
    connect(tableView, &QTableView::doubleClicked, this, &CredentialsWidget::onEditCreds);
    connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection&, const QItemSelection&) {
        tableView->setFocus();
    });
    connect(tableView->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this](int section, Qt::SortOrder order) {
        QString key = sortKeyForCredSection(section);
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

CredentialsWidget::~CredentialsWidget() = default;

void CredentialsWidget::createUI()
{
    credsModel = new CredsTableModel(this);
    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(credsModel);
    proxyModel->setSortRole(Qt::UserRole);

    tableView = new QTableView( this );
    tableView->setModel(proxyModel);
    tableView->setHorizontalHeader(new BoldHeaderView(Qt::Horizontal, tableView));
    tableView->setContextMenuPolicy( Qt::CustomContextMenu );
    tableView->setAutoFillBackground( false );
    tableView->setShowGrid( false );
    tableView->setSortingEnabled( true );
    tableView->setWordWrap( true );
    tableView->setCornerButtonEnabled( false );
    tableView->setSelectionBehavior( QAbstractItemView::SelectRows );
    tableView->setFocusPolicy(Qt::ClickFocus);
    tableView->setAlternatingRowColors( true );
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    tableView->horizontalHeader()->setCascadingSectionResizes( true );
    tableView->horizontalHeader()->setHighlightSections( false );
    tableView->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    tableView->horizontalHeader()->setSortIndicatorShown(true);
    tableView->horizontalHeader()->setSectionsClickable(true);
    tableView->sortByColumn(CC_Date, Qt::DescendingOrder);
    tableView->verticalHeader()->setVisible(false);

    tableView->horizontalHeader()->setSectionResizeMode(CC_Username, QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(CC_Realm,    QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(CC_Type,     QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(CC_Date,     QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(CC_Tag,      QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(CC_Storage,  QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(CC_Agent,    QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(CC_Host,     QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(CC_Password, QHeaderView::Stretch);

    tableView->setItemDelegate(new PaddingDelegate(tableView));
    tableView->setItemDelegateForColumn(CC_Password, new WrapAnywhereDelegate(tableView));

    this->UpdateColumnsVisible();

    pageNavBar = new PageNavBar(this);
    pageNavBar->setFilterPlaceholder("filter: (adm | user) & aes256");

    mainGridLayout = new QGridLayout(this);
    mainGridLayout->setContentsMargins(0, 0, 0, 0);
    mainGridLayout->setVerticalSpacing(4);
    mainGridLayout->setHorizontalSpacing(8);
    mainGridLayout->addWidget(pageNavBar, 0, 0, 1, 1);
    mainGridLayout->addWidget(tableView,  1, 0, 1, 1);
    setLayout(mainGridLayout);
}

void CredentialsWidget::loadCurrentPage()
{
    pageHelper->setParam("agent_id", pageNavBar->currentAgent() == 0 ? QString() : QString::number(pageNavBar->currentAgent()));
    pageHelper->setParam("q",        pageNavBar->filterText());
    pageHelper->setParam("sort",     m_sortCol);
    pageHelper->setParam("order",    m_sortOrder);
    pageHelper->loadPage(m_offset);
}

void CredentialsWidget::onPageReady(const QJsonObject& response)
{
    QJsonArray items = response.value("items").toArray();

    QList<CredentialData> page;
    page.reserve(items.size());

    for (const QJsonValue& v : items) {
        QJsonObject obj = v.toObject();
        CredentialData c;
        c.CredId        = parseI64(obj, "c_creds_id");
        c.Username      = obj["c_username"].toString();
        c.Password      = obj["c_password"].toString();
        c.Realm         = obj["c_realm"].toString();
        c.Type          = obj["c_type"].toString();
        c.Tag           = obj["c_tag"].toString();
        c.DateTimestamp = parseI64(obj, "c_date");
        c.Date          = UnixTimestampGlobalToStringLocal(c.DateTimestamp);
        c.Storage       = obj["c_storage"].toString();
        c.AgentId       = parseI64(obj, "c_agent_id");
        c.Host          = obj["c_host"].toString();
        page.append(c);
    }

    credsModel->reset(page);
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

void CredentialsWidget::onPageError(const QString& message)
{
    if (message.contains("invalid filter", Qt::CaseInsensitive) || message.startsWith("filter:", Qt::CaseInsensitive))
        return;

    credsModel->clear();
    proxyModel->invalidate();
    pageNavBar->setError(message);
    pageNavBar->setPrevEnabled(false);
    pageNavBar->setNextEnabled(false);
    cachePrimed = false;
}

void CredentialsWidget::SetUpdatesEnabled(const bool enabled)
{
    bufferingEnabled = !enabled;
    tableView->setUpdatesEnabled(enabled);
    if (enabled && !cachePrimed)
        loadCurrentPage();
}

/// Main

void CredentialsWidget::AddCredentialsItems(QList<CredentialData> credsList)
{
    if (credsList.isEmpty())
        return;

    QList<CredentialData> filtered;
    {
        QWriteLocker locker(&adaptixWidget->CredentialsLock);
        QSet<qint64> existingIds;
        for (const auto& c : adaptixWidget->Credentials)
            existingIds.insert(c.CredId);

        for (const auto& cred : credsList) {
            if (existingIds.contains(cred.CredId))
                continue;
            existingIds.insert(cred.CredId);
            adaptixWidget->Credentials.push_back(cred);
            filtered.append(cred);
        }
    }

    if (filtered.isEmpty())
        return;

    for (const auto& c : filtered) {
        pageNavBar->addAgent(c.AgentId);
    }
    if (bufferingEnabled)
        return;

    if (cachePrimed)
        loadCurrentPage();
}

void CredentialsWidget::EditCredentialsItem(const CredentialData &newCredentials)
{
    {
        QWriteLocker locker(&adaptixWidget->CredentialsLock);
        for ( int i = 0; i < adaptixWidget->Credentials.size(); i++ ) {
            if( adaptixWidget->Credentials[i].CredId == newCredentials.CredId ) {
                CredentialData* cd = &adaptixWidget->Credentials[i];
                cd->Username = newCredentials.Username;
                cd->Password = newCredentials.Password;
                cd->Realm    = newCredentials.Realm;
                cd->Type     = newCredentials.Type;
                cd->Tag      = newCredentials.Tag;
                cd->Storage  = newCredentials.Storage;
                cd->Host     = newCredentials.Host;
                break;
            }
        }
    }

    if (bufferingEnabled)
        return;

    if (!cachePrimed)
        return;

    if (credsModel->containsId(newCredentials.CredId)) {
        credsModel->update(newCredentials.CredId, newCredentials);
    }
    else {
        loadCurrentPage();
    }
}

void CredentialsWidget::RemoveCredentialsItem(const QList<qint64> &credsId)
{
    QList<qint64> filtered;
    {
        QWriteLocker locker(&adaptixWidget->CredentialsLock);
        for (qint64 credId : credsId) {
            for ( int i = 0; i < adaptixWidget->Credentials.size(); i++ ) {
                if( adaptixWidget->Credentials[i].CredId == credId ) {
                    filtered.append(credId);
                    adaptixWidget->Credentials.erase( adaptixWidget->Credentials.begin() + i );
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

void CredentialsWidget::CredsSetTag(const QList<qint64> &credsIds, const QString &tag)
{
    {
        QWriteLocker locker(&adaptixWidget->CredentialsLock);
        QSet<qint64> set1 = QSet<qint64>(credsIds.begin(), credsIds.end());
        for ( int i = 0; i < adaptixWidget->Credentials.size(); i++ ) {
            if( set1.contains(adaptixWidget->Credentials[i].CredId) ) {
                adaptixWidget->Credentials[i].Tag = tag;
                set1.remove(adaptixWidget->Credentials[i].CredId);
                if (set1.size() == 0)
                    break;
            }
        }
    }
    if (bufferingEnabled)
        return;
    credsModel->setTag(credsIds, tag);
}

void CredentialsWidget::UpdateColumnsSize() const
{
    for (int i = 0; i < CC_ColumnCount; ++i) {
        if (!tableView->isColumnHidden(i) && tableView->horizontalHeader()->sectionResizeMode(i) != QHeaderView::Stretch) {
            tableView->resizeColumnToContents(i);
        }
    }
}

void CredentialsWidget::UpdateColumnsVisible()
{
    if (!tableView || !GlobalClient || !GlobalClient->settings)
        return;
    const auto& cols = GlobalClient->settings->data.CredentialsTableColumns;
    const int map[CC_ColumnCount] = {
        0, // CC_Id
        3, // CC_Username
        5, // CC_Password
        4, // CC_Realm
        1, // CC_Type
        9, // CC_Tag
        2, // CC_Date
        7, // CC_Storage
        8, // CC_Agent
        6, // CC_Host
    };
    for (int i = 0; i < CC_ColumnCount; ++i) {
        if (cols[map[i]])
            tableView->showColumn(i);
        else
            tableView->hideColumn(i);
    }
}


void CredentialsWidget::UpdateFilterComboBoxes() const
{
    QList<qint64> agents;
    {
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        for (const auto& agent : adaptixWidget->AgentsMap)
            if (agent && agent->data.Id != 0)
                agents.append(agent->data.Id);
    }
    pageNavBar->setAgents(agents);
}

void CredentialsWidget::Clear()
{
    {
        QWriteLocker locker(&adaptixWidget->CredentialsLock);
        adaptixWidget->Credentials.clear();
    }

    m_offset = 0;
    cachePrimed = false;
    credsModel->clear();

    pageNavBar->clearAgents();
    pageNavBar->blockSignals(true);
    pageNavBar->clearFilter();
    pageNavBar->blockSignals(false);

    pageNavBar->setInfo(0, 0, 0);
    pageNavBar->setPrevEnabled(false);
    pageNavBar->setNextEnabled(false);
}

/// Sender

void CredentialsWidget::CredentialsAdd(QList<CredentialData> credsList)
{
    QJsonArray jsonArray;
    for (const auto &cred : credsList) {
        QJsonObject obj;
        obj["username"] = cred.Username;
        obj["password"] = cred.Password;
        obj["realm"]    = cred.Realm;
        obj["type"]     = cred.Type;
        obj["tag"]      = cred.Tag;
        obj["storage"]  = cred.Storage;
        obj["host"]     = cred.Host;
        jsonArray.append(obj);
    }

    QJsonObject dataJson;
    dataJson["creds"] = jsonArray;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    HttpReqCredentialsCreateAsync(jsonData, *(adaptixWidget->GetProfile()), [](bool success, const QString &message, const QJsonObject&) {
        if (!success)
            MessageError(message);
    });
}

/// Slots

void CredentialsWidget::handleCredentialsMenu(const QPoint &pos ) const
{
    oclero::qlementine::Menu ctxMenu;
    ctxMenu.addAction("Create", this, &CredentialsWidget::onCreateCreds );

    QModelIndex index = tableView->indexAt(pos);
    if (index.isValid()) {
        QStringList credsStr;
        QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
        for (const QModelIndex &proxyIndex : selectedRows) {
            QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
            if (!sourceIndex.isValid())
                continue;

            qint64 credId = credsModel->data(credsModel->index(sourceIndex.row(), CC_Id), Qt::UserRole).toLongLong();
            credsStr.append(QString::number(credId));
        }

        ctxMenu.addAction("Edit",   this, &CredentialsWidget::onEditCreds );
        ctxMenu.addAction("Remove", this, &CredentialsWidget::onRemoveCreds );
        ctxMenu.addSeparator();

        int centerCount = adaptixWidget->ScriptManager->AddMenuCreds(&ctxMenu, "Creds", credsStr);
        if (centerCount > 0)
            ctxMenu.addSeparator();

        ctxMenu.addAction("Set tag",           this, &CredentialsWidget::onSetTag );
        ctxMenu.addAction("Export to file",    this, &CredentialsWidget::onExportCreds );
        ctxMenu.addAction("Copy to clipboard", this, &CredentialsWidget::onCopyToClipboard );
    }

    QPoint globalPos = tableView->mapToGlobal(pos);
    ctxMenu.exec(globalPos);
}

void CredentialsWidget::onCreateCreds()
{
    DialogCredential* dialogCreds = new DialogCredential();
    while (true) {
        dialogCreds->StartDialog();
        if (dialogCreds->IsValid())
            break;

        QString msg = dialogCreds->GetMessage();
        if (msg.isEmpty()) {
            delete dialogCreds;
            return;
        }

        MessageError(msg);
    }

    CredentialData credData = dialogCreds->GetCredData();

    delete dialogCreds;

    QList<CredentialData> credList;
    credList.append(credData);
    this->CredentialsAdd(credList);
}

void CredentialsWidget::onEditCreds()
{
    auto idx = tableView->currentIndex();
    if (!idx.isValid())
        return;

    qint64 credId = proxyModel->index(idx.row(), CC_Id).data(Qt::UserRole).toLongLong();

    bool found = false;
    CredentialData credentialData;
    {
        QReadLocker locker(&adaptixWidget->CredentialsLock);
        for (auto creds : adaptixWidget->Credentials) {
            if (creds.CredId == credId) {
                credentialData = creds;
                found = true;
                break;
            }
        }
    }
    if (!found)
        return;

    DialogCredential* dialogCreds = new DialogCredential();
    dialogCreds->SetEditmode(credentialData);
    while (true) {
        dialogCreds->StartDialog();
        if (dialogCreds->IsValid())
            break;

        QString msg = dialogCreds->GetMessage();
        if (msg.isEmpty()) {
            delete dialogCreds;
            return;
        }

        MessageError(msg);
    }

    CredentialData credData = dialogCreds->GetCredData();

    QJsonObject dataJson;
    dataJson["cred_id"]  = credData.CredId;
    dataJson["username"] = credData.Username;
    dataJson["password"] = credData.Password;
    dataJson["realm"]    = credData.Realm;
    dataJson["type"]     = credData.Type;
    dataJson["tag"]      = credData.Tag;
    dataJson["storage"]  = credData.Storage;
    dataJson["host"]     = credData.Host;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    delete dialogCreds;

    HttpReqCredentialsEditAsync(jsonData, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Server is not responding" : message);
    });
}

void CredentialsWidget::onRemoveCreds() const
{
    QList<qint64> listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid())
            continue;

        qint64 id = credsModel->data(credsModel->index(sourceIndex.row(), CC_Id), Qt::UserRole).toLongLong();
        listId.append(id);
    }

    if(listId.empty())
        return;

    HttpReqCredentialsRemoveAsync(listId, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void CredentialsWidget::onExportCreds() const
{
    auto idx = tableView->currentIndex();
    if (!idx.isValid())
        return;

    QInputDialog dialog;
    dialog.setWindowTitle("Format for saving");
    dialog.setLabelText("Format:");
    dialog.setTextValue("%realm%\\%username%:%password%");
    QLineEdit *lineEdit = dialog.findChild<QLineEdit*>();
    if (lineEdit) {
        lineEdit->setMinimumWidth(400);
    }

    bool inputOk = (dialog.exec() == QDialog::Accepted);
    if (!inputOk)
        return;

    QString format = dialog.textValue();
    QString baseDir = QStringLiteral("creds.txt");
    if (adaptixWidget && adaptixWidget->GetProfile())
        baseDir = QDir(adaptixWidget->GetProfile()->GetProjectDir()).filePath(QStringLiteral("creds.txt"));

    NonBlockingDialogs::getSaveFileName(const_cast<CredentialsWidget*>(this), "Save credentials", baseDir, "Text Files (*.txt);;All Files (*)",
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

                QString realm    = credsModel->data(credsModel->index(sourceIndex.row(), CC_Realm), Qt::DisplayRole).toString();
                QString username = credsModel->data(credsModel->index(sourceIndex.row(), CC_Username), Qt::DisplayRole).toString();
                QString password = credsModel->data(credsModel->index(sourceIndex.row(), CC_Password), Qt::DisplayRole).toString();

                QString temp = format;
                content += temp
                .replace("%realm%", realm)
                .replace("%username%", username)
                .replace("%password%", password)
                + "\n";
            }

            file.write(content.trimmed().toUtf8());
            file.close();
    });
}

void CredentialsWidget::onSetTag() const
{
    QString tag = "";
    QList<qint64> listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid())
            continue;

        QString cTag = credsModel->data(credsModel->index(sourceIndex.row(), CC_Tag), Qt::DisplayRole).toString();
        qint64 id    = credsModel->data(credsModel->index(sourceIndex.row(), CC_Id),  Qt::UserRole).toLongLong();
        listId.append(id);

        if (tag.isEmpty())
            tag = cTag;
    }

    if(listId.empty())
        return;

    bool inputOk;
    QString newTag = QInputDialog::getText(nullptr, "Set tags", "New tag", QLineEdit::Normal,tag, &inputOk);
    if ( inputOk ) {
        HttpReqCredentialsSetTagAsync(listId, newTag, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void CredentialsWidget::onCopyToClipboard() const
{
    auto idx = tableView->currentIndex();
    if (!idx.isValid()) return;

    QInputDialog dialog;
    dialog.setWindowTitle("Format for clipboard");
    dialog.setComboBoxEditable(true);
    dialog.setTextValue("%realm%\\%username%:%password%");
    dialog.setComboBoxItems(QStringList()
        << "%realm%\\%username%:%password%"
        << "%username%"
        << "%password%"
        << "'%realm%/%username%:%password%' (impacket)"
        << "-hashes :%password% '%realm%/%username%' (impacket)"
        << "-u '%username%' -p '%password%' (netexec)"
        << "-u '%username%' -H '%password%' (netexec)"
        << "-u '%username%@%realm%' -p '%password%' (certipy)"
    );
    dialog.setLabelText("Format:");
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

        QString realm    = credsModel->data(credsModel->index(sourceIndex.row(), CC_Realm), Qt::DisplayRole).toString();
        QString username = credsModel->data(credsModel->index(sourceIndex.row(), CC_Username), Qt::DisplayRole).toString();
        QString password = credsModel->data(credsModel->index(sourceIndex.row(), CC_Password), Qt::DisplayRole).toString();

        QString temp = format;
        content += temp
        .replace("%realm%", realm)
        .replace("%username%", username)
        .replace("%password%", password)
        .replace(" (impacket)", "")
        .replace(" (netexec)", "")
        .replace(" (certipy)", "")
        + "\n";
    }

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(content.trimmed());
}
