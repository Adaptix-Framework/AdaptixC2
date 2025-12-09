#include <UI/Widgets/CredentialsWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <UI/Dialogs/DialogCredential.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Utils/CustomElements.h>
#include <Utils/NonBlockingDialogs.h>

REGISTER_DOCK_WIDGET(CredentialsWidget, "Credentials", true)

CredentialsWidget::CredentialsWidget(AdaptixWidget* w) : DockTab("Credentials", w->GetProfile()->GetProject(), ":/icons/key"), adaptixWidget(w)
{
    this->createUI();

    connect(tableView, &QTableWidget::customContextMenuRequested, this, &CredentialsWidget::handleCredentialsMenu);
    connect(tableView, &QTableWidget::doubleClicked,              this, &CredentialsWidget::onEditCreds);
    connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &selected, const QItemSelection &deselected){
        Q_UNUSED(selected)
        Q_UNUSED(deselected)
        tableView->setFocus();
    });
    connect(hideButton,  &ClickableLabel::clicked, this, &CredentialsWidget::toggleSearchPanel);
    connect(inputFilter, &QLineEdit::textChanged,  this, &CredentialsWidget::onFilterUpdate);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), tableView);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &CredentialsWidget::toggleSearchPanel);

    this->dockWidget->setWidget(this);
}

CredentialsWidget::~CredentialsWidget() = default;

void CredentialsWidget::SetUpdatesEnabled(const bool enabled)
{
    tableView->setUpdatesEnabled(enabled);
}

void CredentialsWidget::createUI()
{
    auto horizontalSpacer2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    searchWidget = new QWidget(this);
    searchWidget->setVisible(false);

    inputFilter = new QLineEdit(searchWidget);
    inputFilter->setPlaceholderText("filter");
    inputFilter->setMaximumWidth(300);

    hideButton = new ClickableLabel("X");
    hideButton->setCursor( Qt::PointingHandCursor );

    searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(0, 5, 0, 0);
    searchLayout->setSpacing(4);
    searchLayout->addWidget(inputFilter);
    searchLayout->addWidget(hideButton);
    searchLayout->addSpacerItem(horizontalSpacer2);

    credsModel = new CredsTableModel(this);
    proxyModel = new CredsFilterProxyModel(this);
    proxyModel->setSourceModel(credsModel);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    tableView = new QTableView( this );
    tableView->setModel(proxyModel);
    tableView->setContextMenuPolicy( Qt::CustomContextMenu );
    tableView->setAutoFillBackground( false );
    tableView->setShowGrid( false );
    tableView->setSortingEnabled( true );
    tableView->setWordWrap( true );
    tableView->setCornerButtonEnabled( false );
    tableView->setSelectionBehavior( QAbstractItemView::SelectRows );
    tableView->setFocusPolicy( Qt::NoFocus );
    tableView->setAlternatingRowColors( true );
    tableView->horizontalHeader()->setSectionResizeMode( QHeaderView::ResizeToContents );
    tableView->horizontalHeader()->setCascadingSectionResizes( true );
    tableView->horizontalHeader()->setHighlightSections( false );
    tableView->verticalHeader()->setVisible( false );

    proxyModel->sort(-1);

    tableView->horizontalHeader()->setSectionResizeMode( CC_Password, QHeaderView::Stretch );
    tableView->horizontalHeader()->setSectionResizeMode( CC_Tag,     QHeaderView::Stretch );

    tableView->setItemDelegate(new PaddingDelegate(tableView));

    tableView->hideColumn(0);

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->setContentsMargins( 0, 0,  0, 0);
    mainGridLayout->addWidget( searchWidget, 0, 0, 1, 1);
    mainGridLayout->addWidget( tableView,  1, 0, 1, 1);
}

/// Main

void CredentialsWidget::AddCredentialsItems(QList<CredentialData> credsList) const
{
    QList<CredentialData> filtered;

    for (auto cred : credsList) {
        for( auto c : adaptixWidget->Credentials ) {
            if( c.CredId == cred.CredId )
                return;
        }

        adaptixWidget->Credentials.push_back(cred);
        filtered.append(cred);
    }

    credsModel->add(filtered);

    if (adaptixWidget->IsSynchronized())
        this->UpdateColumnsSize();
}

void CredentialsWidget::EditCredentialsItem(const CredentialData &newCredentials) const
{
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

    credsModel->update(newCredentials.CredId, newCredentials);
}

void CredentialsWidget::RemoveCredentialsItem(const QStringList &credsId) const
{
    QStringList filtered;
    for (auto credId : credsId) {
        for ( int i = 0; i < adaptixWidget->Credentials.size(); i++ ) {
            if( adaptixWidget->Credentials[i].CredId == credId ) {
                filtered.append(credId);
                adaptixWidget->Credentials.erase( adaptixWidget->Credentials.begin() + i );
                break;
            }
        }
    }
    credsModel->remove(filtered);
}

void CredentialsWidget::CredsSetTag(const QStringList &credsIds, const QString &tag) const
{
    QSet<QString> set1 = QSet<QString>(credsIds.begin(), credsIds.end());
    for ( int i = 0; i < adaptixWidget->Credentials.size(); i++ ) {
        if( set1.contains(adaptixWidget->Credentials[i].CredId) ) {
            adaptixWidget->Credentials[i].Tag = tag;
            set1.remove(adaptixWidget->Credentials[i].CredId);

            if (set1.size() == 0)
                break;
        }
    }

    credsModel->setTag(credsIds, tag);
}

void CredentialsWidget::UpdateColumnsSize() const
{
    tableView->resizeColumnToContents(CC_Username);
    tableView->resizeColumnToContents(CC_Realm);
    tableView->resizeColumnToContents(CC_Type);
    tableView->resizeColumnToContents(CC_Date);
    tableView->resizeColumnToContents(CC_Storage);
    tableView->resizeColumnToContents(CC_Agent);
    tableView->resizeColumnToContents(CC_Host);
}

void CredentialsWidget::Clear() const
{
    adaptixWidget->Credentials.clear();
    credsModel->clear();
    inputFilter->clear();
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

    QString message = "";
    bool ok = false;
    bool result = HttpReqCredentialsCreate(jsonData, *(adaptixWidget->GetProfile()), &message, &ok);
    if( !result ) {
        MessageError("Server is not responding");
        return;
    }
    if (!ok) MessageError(message);
}

/// Slots

void CredentialsWidget::toggleSearchPanel() const
{
    if (this->searchWidget->isVisible()) {
        this->searchWidget->setVisible(false);
        proxyModel->setSearchVisible(false);
    }
    else {
        this->searchWidget->setVisible(true);
        proxyModel->setSearchVisible(true);
    }
}

void CredentialsWidget::onFilterUpdate() const
{
    proxyModel->setTextFilter(inputFilter->text());
}

void CredentialsWidget::handleCredentialsMenu(const QPoint &pos ) const
{
    QModelIndex index = tableView->indexAt(pos);
    if (!index.isValid()) return;

    QStringList creds;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString taskId = credsModel->data(credsModel->index(sourceIndex.row(), CC_Id), Qt::DisplayRole).toString();
        creds.append(taskId);
    }

    auto ctxMenu = QMenu();

    ctxMenu.addAction("Create", this, &CredentialsWidget::onCreateCreds );
    ctxMenu.addAction("Edit",   this, &CredentialsWidget::onEditCreds );
    ctxMenu.addAction("Remove", this, &CredentialsWidget::onRemoveCreds );
    ctxMenu.addSeparator();

    int centerCount = adaptixWidget->ScriptManager->AddMenuCreds(&ctxMenu, "Creds", creds);
    if (centerCount > 0)
        ctxMenu.addSeparator();

    ctxMenu.addAction("Set tag", this, &CredentialsWidget::onSetTag );
    ctxMenu.addAction("Export",  this, &CredentialsWidget::onExportCreds );

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

void CredentialsWidget::onEditCreds() const
{
    auto idx = tableView->currentIndex();
    if (!idx.isValid()) return;

    QString credId = proxyModel->index(idx.row(), CC_Id).data().toString();

    bool found = false;
    CredentialData credentialData;
    for (auto creds : adaptixWidget->Credentials) {
        if (creds.CredId == credId) {
            credentialData = creds;
            found = true;
            break;
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

    QString message = "";
    bool ok = false;
    bool result = HttpReqCredentialsEdit(jsonData, *(adaptixWidget->GetProfile()), &message, &ok);
    if( !result ) {
        MessageError("Server is not responding");
        return;
    }
    if (!ok) MessageError(message);
}

void CredentialsWidget::onRemoveCreds() const
{
    QStringList listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString agentId = credsModel->data(credsModel->index(sourceIndex.row(), CC_Id), Qt::DisplayRole).toString();
        listId.append(agentId);
    }

    if(listId.empty())
        return;

    QString message = QString();
    bool ok = false;
    HttpReqCredentialsRemove(listId, *(adaptixWidget->GetProfile()), &message, &ok);
}

void CredentialsWidget::onExportCreds() const
{
    auto idx = tableView->currentIndex();
    if (!idx.isValid()) return;

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

    NonBlockingDialogs::getSaveFileName(const_cast<CredentialsWidget*>(this), "Save credentials", "creds.txt", "Text Files (*.txt);;All Files (*)",
        [this, format](const QString& fileName) {
            if (fileName.isEmpty())
                return;

            QFile file(fileName);
            if (!file.open(QIODevice::WriteOnly)) {
                MessageError("Failed to open file for writing");
                return;
            }

            QString content = "";
            QStringList listId;
            QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
            for (const QModelIndex &proxyIndex : selectedRows) {
                QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
                if (!sourceIndex.isValid()) continue;

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
    QStringList listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString cTag    = credsModel->data(credsModel->index(sourceIndex.row(), CC_Tag), Qt::DisplayRole).toString();
        QString agentId = credsModel->data(credsModel->index(sourceIndex.row(), CC_Id), Qt::DisplayRole).toString();
        listId.append(agentId);

        if (tag.isEmpty())
            tag = cTag;
    }

    if(listId.empty())
        return;

    bool inputOk;
    QString newTag = QInputDialog::getText(nullptr, "Set tags", "New tag", QLineEdit::Normal,tag, &inputOk);
    if ( inputOk ) {
        QString message = QString();
        bool ok = false;
        bool result = HttpReqCredentialsSetTag(listId, newTag, *(adaptixWidget->GetProfile()), &message, &ok);
        if( !result ) {
            MessageError("Response timeout");
            return;
        }
    }
}