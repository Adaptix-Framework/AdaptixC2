#include <UI/Widgets/CredentialsWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Dialogs/DialogCredential.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Utils/CustomElements.h>

CredentialsWidget::CredentialsWidget(AdaptixWidget* w) : adaptixWidget(w)
{
    this->createUI();

    connect(tableWidget,  &QTableWidget::customContextMenuRequested, this, &CredentialsWidget::handleCredentialsMenu);
    connect(tableWidget,  &QTableWidget::cellDoubleClicked,          this, &CredentialsWidget::onEditCreds);
    connect(tableWidget,  &QTableWidget::itemSelectionChanged,       this, [this](){tableWidget->setFocus();} );
    connect(hideButton,   &ClickableLabel::clicked,                  this, &CredentialsWidget::toggleSearchPanel);
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

    tableWidget->setHorizontalHeaderItem(0, new QTableWidgetItem("CredId"));
    tableWidget->setHorizontalHeaderItem(1, new QTableWidgetItem("Username"));
    tableWidget->setHorizontalHeaderItem(2, new QTableWidgetItem("Password"));
    tableWidget->setHorizontalHeaderItem(3, new QTableWidgetItem("Realm"));
    tableWidget->setHorizontalHeaderItem(4, new QTableWidgetItem("Type"));
    tableWidget->setHorizontalHeaderItem(5, new QTableWidgetItem("Tag"));
    tableWidget->setHorizontalHeaderItem(6, new QTableWidgetItem("Date"));
    tableWidget->setHorizontalHeaderItem(7, new QTableWidgetItem("Storage"));
    tableWidget->setHorizontalHeaderItem(8, new QTableWidgetItem("Agent"));
    tableWidget->setHorizontalHeaderItem(9, new QTableWidgetItem("Host"));

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
    tableWidget->setItem( tableWidget->rowCount() - 1, 0, item_CredId );
    tableWidget->setItem( tableWidget->rowCount() - 1, 1, item_Username );
    tableWidget->setItem( tableWidget->rowCount() - 1, 2, item_Password );
    tableWidget->setItem( tableWidget->rowCount() - 1, 3, item_Realm );
    tableWidget->setItem( tableWidget->rowCount() - 1, 4, item_Type );
    tableWidget->setItem( tableWidget->rowCount() - 1, 5, item_Tag );
    tableWidget->setItem( tableWidget->rowCount() - 1, 6, item_Date );
    tableWidget->setItem( tableWidget->rowCount() - 1, 7, item_Storage );
    tableWidget->setItem( tableWidget->rowCount() - 1, 8, item_Agent );
    tableWidget->setItem( tableWidget->rowCount() - 1, 9, item_Host );
    tableWidget->setSortingEnabled( isSortingEnabled );

    tableWidget->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 3, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 6, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 8, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 9, QHeaderView::ResizeToContents );

    tableWidget->verticalHeader()->setSectionResizeMode(tableWidget->rowCount() - 1, QHeaderView::ResizeToContents);
}

/// Main

void CredentialsWidget::Clear() const
{
    adaptixWidget->Credentials.clear();
    this->ClearTableContent();
    inputFilter->clear();
}

void CredentialsWidget::AddCredentialsItem(const CredentialData &newCredentials) const
{
    for( auto creds : adaptixWidget->Credentials ) {
        if( creds.CredId == newCredentials.CredId )
            return;
    }

    adaptixWidget->Credentials.push_back(newCredentials);

    if( !this->filterItem(newCredentials) )
        return;

    this->addTableItem(newCredentials);
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
            tableWidget->item(row, 1)->setText(newCredentials.Username);
            tableWidget->item(row, 2)->setText(newCredentials.Password);
            tableWidget->item(row, 3)->setText(newCredentials.Realm);
            tableWidget->item(row, 4)->setText(newCredentials.Type);
            tableWidget->item(row, 5)->setText(newCredentials.Tag);
            tableWidget->item(row, 7)->setText(newCredentials.Storage);
            tableWidget->item(row, 9)->setText(newCredentials.Host);
        }
    }
}

void CredentialsWidget::RemoveCredentialsItem(const QString &credId) const
{
    for ( int i = 0; i < adaptixWidget->Credentials.size(); i++ ) {
        if( adaptixWidget->Credentials[i].CredId == credId ) {
            adaptixWidget->Credentials.erase( adaptixWidget->Credentials.begin() + i );
            break;
        }
    }

    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, 0);
        if ( item && item->text() == credId ) {
            tableWidget->removeRow(row);
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

void CredentialsWidget::CredentialsAdd(const QString &username, const QString &password, const QString &realm, const QString &type, const QString &tag, const QString &storage, const QString &host)
{
    QJsonObject dataJson;
    dataJson["username"] = username;
    dataJson["password"] = password;
    dataJson["realm"]    = realm;
    dataJson["type"]     = type;
    dataJson["tag"]      = tag;
    dataJson["storage"]  = storage;
    dataJson["host"]     = host;

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
    auto ctxMenu = QMenu();

    ctxMenu.addAction("Create", this, &CredentialsWidget::onCreateCreds );
    ctxMenu.addAction("Edit",   this, &CredentialsWidget::onEditCreds );
    ctxMenu.addAction("Remove", this, &CredentialsWidget::onRemoveCreds );
    ctxMenu.addSeparator();
    ctxMenu.addAction("Export", this, &CredentialsWidget::onExportCreds );

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

    this->CredentialsAdd(credData.Username, credData.Password, credData.Realm, credData.Type, credData.Tag, credData.Storage, credData.Host);
}

void CredentialsWidget::onEditCreds() const
{
    if (tableWidget->selectionModel()->selectedRows().empty())
        return;

    auto credId = tableWidget->item( tableWidget->currentRow(), 0 )->text();

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
    dataJson["cred_id"] = credData.CredId;
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
        delete dialogCreds;
        return;
    }
    if (!ok) MessageError(message);
}

void CredentialsWidget::onRemoveCreds() const
{
    if (tableWidget->selectionModel()->selectedRows().empty())
        return;

    auto credId = tableWidget->item( tableWidget->currentRow(), 0 )->text();

    QString message = QString();
    bool ok = false;
    bool result = HttpReqCredentialsRemove(credId, *(adaptixWidget->GetProfile()), &message, &ok);
    if( !result ){
        MessageError("Response timeout");
        return;
    }

    if ( !ok ) MessageError(message);
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

    QString fileName = QFileDialog::getSaveFileName( nullptr, "Save credentials", "creds.txt", "Text Files (*.txt);;All Files (*)" );
    if ( fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        MessageError("Failed to open file for writing");
        return;
    }

    QString content = "";
    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( tableWidget->item(rowIndex, 1)->isSelected() ) {

            QString realm    = tableWidget->item(rowIndex, 3)->text();
            QString username = tableWidget->item(rowIndex, 1)->text();
            QString password = tableWidget->item(rowIndex, 2)->text();

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
}
