#include <UI/Widgets/CredentialsFeedWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Dialogs/DialogCredential.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/Settings.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Utils/NonBlockingDialogs.h>
#include <MainAdaptix.h>

#include <oclero/qlementine/widgets/Menu.hpp>

#include <QClipboard>
#include <QInputDialog>


namespace CredsBlock { enum { Id = 0, Text = 1, Main = 2, Tags = 3, Host = 4, Count = 5 }; }

namespace CF { enum { CredId = 0, Type, Created, Username, Realm, Password, Host, Storage, AgentId, Tags, Count }; }

static bool credsCol(int col)
{
    return GlobalClient && GlobalClient->settings && GlobalClient->settings->data.CredentialsTableColumns[col];
}

static FeedRow credToFeedRow(const CredentialData& c)
{
    FeedRow row;
    row.resize(CredsBlock::Count);
    row.entityId = c.CredId;

    QStringList tagList = c.Tag.split(",", Qt::SkipEmptyParts);
    row[CredsBlock::Id] = QVariantMap{
        {"id", credsCol(CF::CredId) ? QString("#%1").arg(c.CredId) : QString()},
        {"idNum", c.CredId},
        {"badge", credsCol(CF::Type) ? c.Type : QString()},
        {"date", credsCol(CF::Created) ? QDateTime::fromSecsSinceEpoch(c.DateTimestamp).toString("dd/MM HH:mm:ss") : QString()},
        {"firstTag", tagList.isEmpty() ? QString() : tagList.first().toLower()}
    };

    row[CredsBlock::Text] = QVariantMap{
            {"main", credsCol(CF::Username) ? c.Username : QString()},
            {"second", credsCol(CF::Realm) ? c.Realm : QString()}
    };

    row[CredsBlock::Main] = QVariantMap{
            {"main", credsCol(CF::Password) ? c.Password : QString()},
            {"submain", QString()},
            {"second", QString()},
            {"realm", c.Realm.toLower()},
            {"username", c.Username.toLower()},
            {"type", c.Type.toLower()},
            {"host", c.Host.toLower()},
            {"dateNum", c.DateTimestamp},
            {"usernameFull", c.Username},
            {"passwordFull", c.Password},
            {"realmFull", c.Realm}
    };

    row[CredsBlock::Tags] = credsCol(CF::Tags) ? tagList : QStringList();

    QString hostMain;
    if (credsCol(CF::Host) && !c.Host.isEmpty()) {
        if (credsCol(CF::AgentId) && c.AgentId > 0)
            hostMain = QString("%1 (#%2)").arg(c.Host).arg(c.AgentId);
        else
            hostMain = c.Host;
    } else if (credsCol(CF::AgentId) && c.AgentId > 0) {
        hostMain = QString("(#%1)").arg(c.AgentId);
    }

    row[CredsBlock::Host] = QVariantMap{
            {"main", hostMain},
            {"second", credsCol(CF::Storage) ? c.Storage : QString()}
    };

    row.isDead = false;
    return row;
}

static ListFeedDelegate* createCredsDelegate(QObject* parent)
{
    auto* d = new ListFeedDelegate(parent);
    d->addBlock(new IdBadgeBlock());
    d->addBlock(new TextBlock());
    d->addBlock(new MainBlock());
    d->addBlock(new TagsBlock());
    d->addBlock(new TextBlock());
    d->addBlock(new GroupHeaderBlock());
    return d;
}



CredentialsFeedWidget::CredentialsFeedWidget(AdaptixWidget* w) : ListFeedWidget(w), m_adaptixWidget(w)
{
    feedBlockModel = new FeedListModel(this);
    auto* delegate = createCredsDelegate(this);
    delegate->setFeedModel(feedBlockModel);

    setModel(feedBlockModel);
    setDelegate(delegate);
    rebuildModelChain();

    enableSearch(true);
    enableAutoCheck(true);
    enableFilterCombo(true, "All types");
    enableSortingCombo(true, {"No sorting", "Date", "Realm", "Username", "Type", "Host"});
    finalizeSearchWidget();

    enablePagination(true);

    auto* addBtn = new QPushButton("+ Add Credential", this);
    connect(addBtn, &QPushButton::clicked, this, &CredentialsFeedWidget::onCreateCreds);
    addToolbarWidgetAfter(addBtn);

    enableCompactSwitch(true);
    if (GlobalClient && GlobalClient->settings)
        setCompactMode(GlobalClient->settings->data.CredentialsCompactMode);
    setBlockGap(12);
    setTagSize(11, 20);
    setIconSizes(22, 18);

    dockWidget = new KDDockWidgets::QtWidgets::DockWidget( "CredsFeed:Dock-" + w->GetProfile()->GetProject(), KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    dockWidget->setTitle("Credentials");
    dockWidget->setWidget(this);
    dockWidget->setIcon(QIcon(":/icons/key"), KDDockWidgets::IconPlace::TabBar);

    setupPagination();

    connect(treeView(), &QTreeView::customContextMenuRequested, this, &CredentialsFeedWidget::handleFeedMenu);
    connect(treeView(), &QTreeView::doubleClicked, this, &CredentialsFeedWidget::onItemDoubleClicked);
}

CredentialsFeedWidget::~CredentialsFeedWidget() = default;

KDDockWidgets::QtWidgets::DockWidget* CredentialsFeedWidget::dock() { return dockWidget; }

void CredentialsFeedWidget::setupPagination()
{
    pageHelper = new PagedTableHelper(m_adaptixWidget->GetProfile(), "/creds/list", this);
    pageHelper->setPageSize(paginationBar()->pageSize());

    connect(pageHelper, &PagedTableHelper::pageReady, this, &CredentialsFeedWidget::onPageReady);
    connect(pageHelper, &PagedTableHelper::errorOccurred, this, &CredentialsFeedWidget::onPageError);
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

void CredentialsFeedWidget::loadCurrentPage()
{
    if (searchInput())
        pageHelper->setParam("q", searchInput()->text());
    if (filterCombo() && filterCombo()->currentIndex() > 0)
        pageHelper->setParam("type", filterCombo()->currentData().toString());
    else
        pageHelper->setParam("type", "");
    pageHelper->setParam("sort", m_sortCol);
    pageHelper->setParam("order", m_sortOrder);
    pageHelper->loadPage(m_offset);
}

void CredentialsFeedWidget::onFilterChanged()
{
    if (autoAction() && autoAction()->isChecked()) {
        m_offset = 0;
        loadCurrentPage();
    }
}

void CredentialsFeedWidget::onSortingChanged(int index)
{
    switch (index) {
        case 0: m_sortCol = "Date";     m_sortOrder = "desc"; break;
        case 1: m_sortCol = "Date";     m_sortOrder = isSortAscending() ? "asc" : "desc"; break;
        case 2: m_sortCol = "Realm";    m_sortOrder = isSortAscending() ? "asc" : "desc"; break;
        case 3: m_sortCol = "Username";  m_sortOrder = isSortAscending() ? "asc" : "desc"; break;
        case 4: m_sortCol = "Type";     m_sortOrder = isSortAscending() ? "asc" : "desc"; break;
        case 5: m_sortCol = "Host";     m_sortOrder = isSortAscending() ? "asc" : "desc"; break;
        default: return;
    }
    m_offset = 0;
    loadCurrentPage();
}

void CredentialsFeedWidget::onPageReady(const QJsonObject& response)
{
    QJsonArray items = response.value("items").toArray();
    QList<CredentialData> newCreds;
    for (const auto& itemVal : items) {
        QJsonObject obj = itemVal.toObject();
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
        newCreds.append(c);
    }

    {
        QWriteLocker locker(&m_adaptixWidget->CredentialsLock);
        QSet<qint64> existingIds;
        for (const auto& ec : m_adaptixWidget->Credentials)
            existingIds.insert(ec.CredId);
        for (const auto& nc : newCreds) {
            if (!existingIds.contains(nc.CredId)) {
                m_adaptixWidget->Credentials.push_back(nc);
                existingIds.insert(nc.CredId);
            }
        }
    }

    m_credCache.clear();
    feedBlockModel->clear();
    QSet<QString> types;
    for (const auto& c : newCreds) {
        m_credCache[c.CredId] = c;
        FeedRow row = credToFeedRow(c);
        feedBlockModel->addRow(row);
        if (!c.Type.isEmpty())
            types.insert(c.Type);
    }

    if (filterCombo()) {
        filterCombo()->blockSignals(true);
        QString current = filterCombo()->currentText();
        filterCombo()->clear();
        filterCombo()->addItem("All types", "");
        for (const auto& t : types)
            filterCombo()->addItem(t, t);
        int idx = filterCombo()->findText(current);
        if (idx >= 0)
            filterCombo()->setCurrentIndex(idx);
        filterCombo()->blockSignals(false);
    }

    int total = response["total"].toInt();
    int shown = newCreds.size();
    int from  = shown == 0 ? 0 : m_offset + 1;
    int to    = m_offset + shown;
    paginationBar()->setInfo(from, to, total);
    paginationBar()->setPrevEnabled(m_offset > 0);
    paginationBar()->setNextEnabled(m_offset + shown < total);

    cachePrimed = true;
}

void CredentialsFeedWidget::onPageError(const QString& message)
{
    Q_UNUSED(message);
    m_credCache.clear();
    feedBlockModel->clear();
    paginationBar()->setInfo(0, 0, 0);
    paginationBar()->setPrevEnabled(false);
    paginationBar()->setNextEnabled(false);
    cachePrimed = false;
}

void CredentialsFeedWidget::SetUpdatesEnabled(bool enabled)
{
    treeView()->setUpdatesEnabled(enabled);
    if (enabled && !cachePrimed)
        loadCurrentPage();
}

void CredentialsFeedWidget::Clear()
{
    m_credCache.clear();
    if (feedBlockModel)
        feedBlockModel->clear();

    m_offset = 0;
    cachePrimed = false;

    if (filterCombo()) {
        filterCombo()->blockSignals(true);
        filterCombo()->clear();
        filterCombo()->addItem("All types", "");
        filterCombo()->blockSignals(false);
    }

    paginationBar()->setInfo(0, 0, 0);
    paginationBar()->setPrevEnabled(false);
    paginationBar()->setNextEnabled(false);
}

void CredentialsFeedWidget::UpdateColumnsVisible()
{
    if (!feedBlockModel)
        return;
    for (int i = 0; i < feedBlockModel->size(); ++i) {
        const FeedRow& r = feedBlockModel->rowAt(i);
        auto it = m_credCache.constFind(r.entityId);
        if (it == m_credCache.constEnd())
            continue;
        feedBlockModel->updateRow(i, credToFeedRow(it.value()));
    }
    auto* delegate = qobject_cast<ListFeedDelegate*>(treeView()->itemDelegate());
    if (delegate)
        delegate->updateMaxWidths(feedBlockModel);
}



void CredentialsFeedWidget::AddCredentialsItems(QList<CredentialData> credsList)
{
    if (credsList.isEmpty())
        return;

    QList<CredentialData> filtered;
    {
        QWriteLocker locker(&m_adaptixWidget->CredentialsLock);
        QSet<qint64> existingIds;
        for (const auto& c : m_adaptixWidget->Credentials)
            existingIds.insert(c.CredId);

        for (const auto& cred : credsList) {
            if (existingIds.contains(cred.CredId))
                continue;
            existingIds.insert(cred.CredId);
            m_adaptixWidget->Credentials.push_back(cred);
            filtered.append(cred);
        }
    }

    if (filtered.isEmpty())
        return;

    if (cachePrimed) {
        m_offset = 0;
        loadCurrentPage();
    }
}

void CredentialsFeedWidget::EditCredentialsItem(const CredentialData& newCredentials)
{
    {
        QWriteLocker locker(&m_adaptixWidget->CredentialsLock);
        for (int i = 0; i < m_adaptixWidget->Credentials.size(); i++) {
            if (m_adaptixWidget->Credentials[i].CredId == newCredentials.CredId) {
                CredentialData* cd = &m_adaptixWidget->Credentials[i];
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

    m_credCache[newCredentials.CredId] = newCredentials;

    if (!cachePrimed)
        return;

    for (int i = 0; i < feedBlockModel->size(); ++i) {
        const FeedRow& r = feedBlockModel->rowAt(i);
        if (r.entityId == newCredentials.CredId) {
            FeedRow newRow = credToFeedRow(newCredentials);
            feedBlockModel->updateRow(i, newRow);
            return;
        }
    }
}

void CredentialsFeedWidget::RemoveCredentialsItem(const QList<qint64>& credsId)
{
    {
        QWriteLocker locker(&m_adaptixWidget->CredentialsLock);
        for (qint64 credId : credsId) {
            for (int i = 0; i < m_adaptixWidget->Credentials.size(); i++) {
                if (m_adaptixWidget->Credentials[i].CredId == credId) {
                    m_adaptixWidget->Credentials.erase(m_adaptixWidget->Credentials.begin() + i);
                    break;
                }
            }
        }
    }

    if (!cachePrimed)
        return;

    loadCurrentPage();
}

void CredentialsFeedWidget::CredsSetTag(const QList<qint64>& credsIds, const QString& tag)
{
    {
        QWriteLocker locker(&m_adaptixWidget->CredentialsLock);
        QSet<qint64> idSet(credsIds.begin(), credsIds.end());
        for (int i = 0; i < m_adaptixWidget->Credentials.size(); i++) {
            if (idSet.contains(m_adaptixWidget->Credentials[i].CredId)) {
                m_adaptixWidget->Credentials[i].Tag = tag;
                idSet.remove(m_adaptixWidget->Credentials[i].CredId);
                if (idSet.isEmpty())
                    break;
            }
        }
    }

    if (!cachePrimed)
        return;

    loadCurrentPage();
}



void CredentialsFeedWidget::CredentialsAdd(QList<CredentialData> credsList)
{
    QJsonArray jsonArray;
    for (const auto& cred : credsList) {
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

    HttpReqCredentialsCreateAsync(jsonData, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message);
    });
}



CredentialsFeedWidget::CredInfo CredentialsFeedWidget::currentCredInfo() const
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
    CredInfo info;
    info.credId = r.entityId;
    auto it = m_credCache.constFind(info.credId);
    if (it != m_credCache.constEnd()) {
        info.realm    = it->Realm;
        info.username = it->Username;
        info.password = it->Password;
    } else {
        const QVariantMap mainMap = r.blockData[CredsBlock::Main].toMap();
        info.realm    = mainMap.value("realmFull").toString();
        info.username = mainMap.value("usernameFull").toString();
        info.password = mainMap.value("passwordFull").toString();
        if (info.username.isEmpty())
            info.username = r.blockData[CredsBlock::Text].toMap()["main"].toString();
        if (info.password.isEmpty())
            info.password = r.blockData[CredsBlock::Main].toMap()["main"].toString();
        if (info.realm.isEmpty())
            info.realm = r.blockData[CredsBlock::Text].toMap()["second"].toString();
    }
    info.valid = info.credId > 0;
    return info;
}



void CredentialsFeedWidget::onItemDoubleClicked(const QModelIndex& index)
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
        onEditCreds();
}

void CredentialsFeedWidget::handleFeedMenu(const QPoint& pos)
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
            qint64 credId = row.entityId;
            if (credId > 0) {
                QStringList credsStr;
                QModelIndexList selectedRows = treeView()->selectionModel()->selectedRows();
                for (const QModelIndex& selIdx : selectedRows) {
                    qint64 selId = selIdx.data(Qt::UserRole).toLongLong();
                    if (selId > 0)
                        credsStr.append(QString::number(selId));
                }
                if (credsStr.isEmpty())
                    credsStr.append(QString::number(credId));

                ctxMenu.addAction("Edit",   this, &CredentialsFeedWidget::onEditCreds);
                ctxMenu.addAction("Remove", this, &CredentialsFeedWidget::onRemoveCreds);
                ctxMenu.addSeparator();

                int centerCount = m_adaptixWidget->ScriptManager->AddMenuCreds(&ctxMenu, "Creds", credsStr);
                if (centerCount > 0)
                    ctxMenu.addSeparator();

                ctxMenu.addAction("Set tag",           this, &CredentialsFeedWidget::onSetTag);
                ctxMenu.addAction("Export to file",    this, &CredentialsFeedWidget::onExportCreds);
                ctxMenu.addAction("Copy to clipboard", this, &CredentialsFeedWidget::onCopyToClipboard);
            }
        }
    }

    ctxMenu.exec(treeView()->viewport()->mapToGlobal(pos));
}

void CredentialsFeedWidget::onCreateCreds()
{
    if (!m_adaptixWidget)
        return;

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

void CredentialsFeedWidget::onEditCreds()
{
    CredInfo info = currentCredInfo();
    if (!info.valid)
        return;

    CredentialData credentialData;
    bool found = false;
    {
        QReadLocker locker(&m_adaptixWidget->CredentialsLock);
        for (const auto& c : m_adaptixWidget->Credentials) {
            if (c.CredId == info.credId) {
                credentialData = c;
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

    HttpReqCredentialsEditAsync(jsonData, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Server is not responding" : message);
    });
}

void CredentialsFeedWidget::onRemoveCreds()
{
    CredInfo info = currentCredInfo();
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
        listId.append(info.credId);

    HttpReqCredentialsRemoveAsync(listId, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void CredentialsFeedWidget::onSetTag()
{
    CredInfo info = currentCredInfo();
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
                    currentTag = r.blockData[CredsBlock::Tags].toStringList().join(",");
                }
            }
        }
    }
    if (listId.isEmpty())
        return;

    bool inputOk;
    QString newTag = QInputDialog::getText(nullptr, "Set tags", "New tag", QLineEdit::Normal, currentTag, &inputOk);
    if (inputOk) {
        HttpReqCredentialsSetTagAsync(listId, newTag, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void CredentialsFeedWidget::onExportCreds()
{
    QModelIndexList selectedRows = treeView()->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
        return;

    QInputDialog dialog;
    dialog.setWindowTitle("Format for saving");
    dialog.setLabelText("Format:");
    dialog.setTextValue("%realm%\\%username%:%password%");
    QLineEdit* lineEdit = dialog.findChild<QLineEdit*>();
    if (lineEdit)
        lineEdit->setMinimumWidth(400);

    bool inputOk = (dialog.exec() == QDialog::Accepted);
    if (!inputOk)
        return;

    QString format = dialog.textValue();

    QString baseDir = QStringLiteral("creds.txt");
    if (m_adaptixWidget && m_adaptixWidget->GetProfile())
        baseDir = QDir(m_adaptixWidget->GetProfile()->GetProjectDir()).filePath(QStringLiteral("creds.txt"));

    NonBlockingDialogs::getSaveFileName(this, "Save credentials", baseDir, "Text Files (*.txt);;All Files (*)",
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
                QString realm, username, password;
                auto it = m_credCache.constFind(r.entityId);
                if (it != m_credCache.constEnd()) {
                    realm = it->Realm; username = it->Username; password = it->Password;
                } else {
                    realm    = r.blockData[CredsBlock::Text].toMap()["second"].toString();
                    username = r.blockData[CredsBlock::Text].toMap()["main"].toString();
                    password = r.blockData[CredsBlock::Main].toMap()["main"].toString();
                }

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

void CredentialsFeedWidget::onCopyToClipboard()
{
    QModelIndexList selectedRows = treeView()->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
        return;

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
        QString realm, username, password;
        auto it = m_credCache.constFind(r.entityId);
        if (it != m_credCache.constEnd()) {
            realm = it->Realm; username = it->Username; password = it->Password;
        } else {
            realm    = r.blockData[CredsBlock::Text].toMap()["second"].toString();
            username = r.blockData[CredsBlock::Text].toMap()["main"].toString();
            password = r.blockData[CredsBlock::Main].toMap()["main"].toString();
        }

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

    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(content.trimmed());
}
