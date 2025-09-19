#include <UI/Widgets/CredentialsWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Dialogs/DialogCredential.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Utils/CustomElements.h>
#include <Utils/NonBlockingDialogs.h>

CredentialsWidget::CredentialsWidget(AdaptixWidget* w) : adaptixWidget(w)
{
    this->createUI();

    connect(tableWidget, &QTableWidget::customContextMenuRequested, this, &CredentialsWidget::handleCredentialsMenu);
    connect(tableWidget, &QTableWidget::cellDoubleClicked,          this, &CredentialsWidget::onEditCreds);
    connect(tableWidget, &QTableWidget::itemSelectionChanged,       this, [this](){tableWidget->setFocus();} );
    connect(hideButton,  &ClickableLabel::clicked,                  this, &CredentialsWidget::toggleSearchPanel);
    connect(inputFilter, &QLineEdit::textChanged,                   this, &CredentialsWidget::onFilterUpdate);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), tableWidget);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &CredentialsWidget::toggleSearchPanel);
}

CredentialsWidget::~CredentialsWidget() = default;

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

    tableWidget = new QTableWidget( this );
    tableWidget->setColumnCount( 10 );
    tableWidget->setContextMenuPolicy( Qt::CustomContextMenu );
    tableWidget->setAutoFillBackground( false );
    tableWidget->setShowGrid( false );
    tableWidget->setSortingEnabled( true );
    tableWidget->setWordWrap( true );
    tableWidget->setCornerButtonEnabled( false );
    tableWidget->setSelectionBehavior( QAbstractItemView::SelectRows );
    tableWidget->setFocusPolicy( Qt::NoFocus );
    tableWidget->setAlternatingRowColors( true );
    tableWidget->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    tableWidget->horizontalHeader()->setCascadingSectionResizes( true );
    tableWidget->horizontalHeader()->setHighlightSections( false );
    tableWidget->verticalHeader()->setVisible( false );

    tableWidget->setHorizontalHeaderItem(ColumnId,       new QTableWidgetItem("CredId"));
    tableWidget->setHorizontalHeaderItem(ColumnUsername, new QTableWidgetItem("Username"));
    tableWidget->setHorizontalHeaderItem(ColumnPassword, new QTableWidgetItem("Password"));
    tableWidget->setHorizontalHeaderItem(ColumnRealm,    new QTableWidgetItem("Realm"));
    tableWidget->setHorizontalHeaderItem(ColumnType,     new QTableWidgetItem("Type"));
    tableWidget->setHorizontalHeaderItem(ColumnTag,      new QTableWidgetItem("Tag"));
    tableWidget->setHorizontalHeaderItem(ColumnDate,     new QTableWidgetItem("Date"));
    tableWidget->setHorizontalHeaderItem(ColumnStorage,  new QTableWidgetItem("Storage"));
    tableWidget->setHorizontalHeaderItem(ColumnAgent,    new QTableWidgetItem("Agent"));
    tableWidget->setHorizontalHeaderItem(ColumnHost,     new QTableWidgetItem("Host"));

    tableWidget->setItemDelegate(new PaddingDelegate(tableWidget));

    tableWidget->hideColumn(0);

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->setContentsMargins( 0, 0,  0, 0);
    mainGridLayout->addWidget( searchWidget, 0, 0, 1, 1);
    mainGridLayout->addWidget( tableWidget,  1, 0, 1, 1);
}

bool CredentialsWidget::filterItem(const CredentialData &credentials) const
{
    if ( !this->searchWidget->isVisible() )
        return true;

    QString filter1 = this->inputFilter->text();
    if( !filter1.isEmpty() ) {
        if ( credentials.Username.contains(filter1, Qt::CaseInsensitive) ||
             credentials.Password.contains(filter1, Qt::CaseInsensitive) ||
             credentials.Realm.contains(filter1, Qt::CaseInsensitive) ||
             credentials.Type.contains(filter1, Qt::CaseInsensitive) ||
             credentials.Tag.contains(filter1, Qt::CaseInsensitive) ||
             credentials.Storage.contains(filter1, Qt::CaseInsensitive) ||
             credentials.Host.contains(filter1, Qt::CaseInsensitive) ||
             credentials.AgentId.contains(filter1, Qt::CaseInsensitive)
        )
            return true;
        else
            return false;
    }
    return true;
}

void CredentialsWidget::addTableItem(const CredentialData &newCredentials) const
{
    auto item_CredId   = new QTableWidgetItem( newCredentials.CredId );
    auto item_Username = new QTableWidgetItem( newCredentials.Username );
    auto item_Password = new QTableWidgetItem( newCredentials.Password );
    auto item_Realm    = new QTableWidgetItem( newCredentials.Realm );
    auto item_Type     = new QTableWidgetItem( newCredentials.Type );
    auto item_Tag      = new QTableWidgetItem( newCredentials.Tag );
    auto item_Date     = new QTableWidgetItem( newCredentials.Date );
    auto item_Storage  = new QTableWidgetItem( newCredentials.Storage );
    auto item_Agent    = new QTableWidgetItem( newCredentials.AgentId );
    auto item_Host     = new QTableWidgetItem( newCredentials.Host );

    item_Username->setFlags( item_Username->flags() ^ Qt::ItemIsEditable );
    item_Username->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );

    item_Password->setFlags( item_Password->flags() ^ Qt::ItemIsEditable );
    item_Password->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );

    item_Realm->setFlags( item_Realm->flags() ^ Qt::ItemIsEditable );
    item_Realm->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );

    item_Type->setFlags( item_Type->flags() ^ Qt::ItemIsEditable );
    item_Type->setTextAlignment( Qt::AlignCenter );

    item_Tag->setFlags( item_Tag->flags() ^ Qt::ItemIsEditable );
    item_Tag->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );

    item_Date->setFlags( item_Date->flags() ^ Qt::ItemIsEditable );
    item_Date->setTextAlignment( Qt::AlignCenter );

    item_Storage->setFlags( item_Storage->flags() ^ Qt::ItemIsEditable );
    item_Storage->setTextAlignment( Qt::AlignCenter );

    item_Agent->setFlags( item_Agent->flags() ^ Qt::ItemIsEditable );
    item_Agent->setTextAlignment( Qt::AlignCenter );

    item_Host->setFlags( item_Host->flags() ^ Qt::ItemIsEditable );
    item_Host->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );

    if( tableWidget->rowCount() < 1 )
        tableWidget->setRowCount( 1 );
    else
        tableWidget->setRowCount( tableWidget->rowCount() + 1 );

    bool isSortingEnabled = tableWidget->isSortingEnabled();
    tableWidget->setSortingEnabled( false );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnId,       item_CredId );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnUsername, item_Username );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnPassword, item_Password );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnRealm,    item_Realm );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnType,     item_Type );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnTag,      item_Tag );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnDate,     item_Date );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnStorage,  item_Storage );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnAgent,    item_Agent );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnHost,     item_Host );
    tableWidget->setSortingEnabled( isSortingEnabled );

    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnUsername, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnPassword, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnRealm,    QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnDate,     QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnAgent,    QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnHost,     QHeaderView::ResizeToContents );

    tableWidget->verticalHeader()->setSectionResizeMode(tableWidget->rowCount() - 1, QHeaderView::ResizeToContents);
}

/// Main

void CredentialsWidget::Clear() const
{
    adaptixWidget->Credentials.clear();
    this->ClearTableContent();
    inputFilter->clear();
}

void CredentialsWidget::AddCredentialsItems(QList<CredentialData> credsList) const
{
    for (auto cred : credsList) {
        for( auto c : adaptixWidget->Credentials ) {
            if( c.CredId == cred.CredId )
                return;
        }

        adaptixWidget->Credentials.push_back(cred);

        if( !this->filterItem(cred) )
            return;

        this->addTableItem(cred);
    }
}

void CredentialsWidget::EditCredentialsItem(const CredentialData &newCredentials) const
{
    for ( int i = 0; i < adaptixWidget->Credentials.size(); i++ ) {
        if( adaptixWidget->Credentials[i].CredId == newCredentials.CredId ) {
            adaptixWidget->Credentials[i].Username = newCredentials.Username;
            adaptixWidget->Credentials[i].Password = newCredentials.Password;
            adaptixWidget->Credentials[i].Realm    = newCredentials.Realm;
            adaptixWidget->Credentials[i].Type     = newCredentials.Type;
            adaptixWidget->Credentials[i].Tag      = newCredentials.Tag;
            adaptixWidget->Credentials[i].Storage  = newCredentials.Storage;
            adaptixWidget->Credentials[i].Host     = newCredentials.Host;
            break;
        }
    }

    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, 0);
        if ( item && item->text() == newCredentials.CredId ) {
            tableWidget->item(row, ColumnUsername)->setText(newCredentials.Username);
            tableWidget->item(row, ColumnPassword)->setText(newCredentials.Password);
            tableWidget->item(row, ColumnRealm)->setText(newCredentials.Realm);
            tableWidget->item(row, ColumnType)->setText(newCredentials.Type);
            tableWidget->item(row, ColumnTag)->setText(newCredentials.Tag);
            tableWidget->item(row, ColumnStorage)->setText(newCredentials.Storage);
            tableWidget->item(row, ColumnHost)->setText(newCredentials.Host);
        }
    }
}

void CredentialsWidget::RemoveCredentialsItem(const QStringList &credsId) const
{
    for (auto credId : credsId) {
        for ( int i = 0; i < adaptixWidget->Credentials.size(); i++ ) {
            if( adaptixWidget->Credentials[i].CredId == credId ) {
                adaptixWidget->Credentials.erase( adaptixWidget->Credentials.begin() + i );
                break;
            }
        }

        for (int row = 0; row < tableWidget->rowCount(); ++row) {
            QTableWidgetItem *item = tableWidget->item(row, ColumnId);
            if ( item && item->text() == credId ) {
                tableWidget->removeRow(row);
                break;
            }
        }
    }
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

    QSet<QString> set2 = QSet<QString>(credsIds.begin(), credsIds.end());
    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, ColumnId);
        if( item && set2.contains(item->text()) ) {
            tableWidget->item(row, ColumnTag)->setText(tag);
            set2.remove(item->text());

            if (set2.size() == 0)
                break;
        }
    }
}

void CredentialsWidget::SetData() const
{
    this->ClearTableContent();

    for (int i = 0; i < adaptixWidget->Credentials.size(); i++ ) {
        if ( this->filterItem(adaptixWidget->Credentials[i]) )
            this->addTableItem(adaptixWidget->Credentials[i]);
    }
}

void CredentialsWidget::ClearTableContent() const
{
    for (int row = tableWidget->rowCount() - 1; row >= 0; row--) {
        for (int col = 0; col < tableWidget->columnCount(); ++col)
            tableWidget->takeItem(row, col);

        tableWidget->removeRow(row);
    }
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
    if (this->searchWidget->isVisible())
        this->searchWidget->setVisible(false);
    else
        this->searchWidget->setVisible(true);

    this->SetData();
}

void CredentialsWidget::onFilterUpdate() const { this->SetData(); }

void CredentialsWidget::handleCredentialsMenu(const QPoint &pos ) const
{
    QStringList creds;
    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( tableWidget->item(rowIndex, 0)->isSelected() ) {
            QString credId = tableWidget->item( rowIndex, ColumnId )->text();
            creds.append(credId);
        }
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

    QPoint globalPos = tableWidget->mapToGlobal(pos);
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
    if (tableWidget->selectionModel()->selectedRows().empty())
        return;

    auto credId = tableWidget->item( tableWidget->currentRow(), ColumnId )->text();

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
    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( tableWidget->item(rowIndex, 0)->isSelected() ) {
            auto id = tableWidget->item( rowIndex, ColumnId )->text();
            listId.append(id);
        }
    }

    if(listId.empty())
        return;

    QString message = QString();
    bool ok = false;
    HttpReqCredentialsRemove(listId, *(adaptixWidget->GetProfile()), &message, &ok);
}

void CredentialsWidget::onExportCreds() const
{
    if (tableWidget->selectionModel()->selectedRows().empty())
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
            for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
                if ( tableWidget->item(rowIndex, 1)->isSelected() ) {

                    QString realm    = tableWidget->item(rowIndex, ColumnRealm)->text();
                    QString username = tableWidget->item(rowIndex, ColumnUsername)->text();
                    QString password = tableWidget->item(rowIndex, ColumnPassword)->text();

                    QString temp = format;
                    content += temp
                    .replace("%realm%", realm)
                    .replace("%username%", username)
                    .replace("%password%", password)
                    + "\n";
                }
            }

            file.write(content.trimmed().toUtf8());
            file.close();
        });
}

void CredentialsWidget::onSetTag() const
{
    QStringList listId;
    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( tableWidget->item(rowIndex, ColumnId)->isSelected() ) {
            auto agentId = tableWidget->item( rowIndex, ColumnId )->text();
            listId.append(agentId);
        }
    }

    if(listId.empty())
        return;

    QString tag = "";
    if(listId.size() == 1) {
        tag = tableWidget->item( tableWidget->currentRow(), ColumnTag )->text();
    }

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