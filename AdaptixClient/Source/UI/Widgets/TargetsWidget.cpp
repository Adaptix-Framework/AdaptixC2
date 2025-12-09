#include <UI/Widgets/TargetsWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/WidgetRegistry.h>
#include <UI/Dialogs/DialogTarget.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Utils/CustomElements.h>
#include <Utils/NonBlockingDialogs.h>

REGISTER_DOCK_WIDGET(TargetsWidget, "Targets")

TargetsWidget::TargetsWidget(AdaptixWidget* w) : DockTab("Targets", w->GetProfile()->GetProject(), ":/icons/devices"), adaptixWidget(w)
{
    this->createUI();

    connect(tableView,  &QTableWidget::customContextMenuRequested, this, &TargetsWidget::handleTargetsMenu);
    connect(tableView,  &QTableWidget::doubleClicked,              this, &TargetsWidget::onEditTarget);
    connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &selected, const QItemSelection &deselected){
        Q_UNUSED(selected)
        Q_UNUSED(deselected)
        tableView->setFocus();
    });
    connect(hideButton,   &ClickableLabel::clicked, this, &TargetsWidget::toggleSearchPanel);
    connect(inputFilter,  &QLineEdit::textChanged,  this, &TargetsWidget::onFilterUpdate);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), tableView);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &TargetsWidget::toggleSearchPanel);

    this->dockWidget->setWidget(this);
}

TargetsWidget::~TargetsWidget() = default;

void TargetsWidget::SetUpdatesEnabled(const bool enabled)
{
    tableView->setUpdatesEnabled(enabled);
}

void TargetsWidget::createUI()
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

    targetsModel = new TargetsTableModel(this);
    proxyModel = new TargetsFilterProxyModel(this);
    proxyModel->setSourceModel(targetsModel);
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

    tableView->horizontalHeader()->setSectionResizeMode( TRC_Tag,  QHeaderView::Stretch );
    tableView->horizontalHeader()->setSectionResizeMode( TRC_Os,   QHeaderView::Stretch );
    tableView->horizontalHeader()->setSectionResizeMode( TRC_Info, QHeaderView::Stretch );

    tableView->setItemDelegate(new PaddingDelegate(tableView));

    tableView->hideColumn(0);

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->setContentsMargins( 0, 0,  0, 0);
    mainGridLayout->addWidget( searchWidget, 0, 0, 1, 1);
    mainGridLayout->addWidget( tableView,  1, 0, 1, 1);
}

/// Main

void TargetsWidget::AddTargetsItems(QList<TargetData> targetList) const
{
    QList<TargetData> filtered;

    for (auto target : targetList) {
        for( auto t : adaptixWidget->Targets ) {
            if( t.TargetId == target.TargetId )
                continue;
        }

        adaptixWidget->Targets.push_back(target);
        filtered.append(target);
    }

    targetsModel->add(filtered);

    if (adaptixWidget->IsSynchronized())
        this->UpdateColumnsSize();
}

void TargetsWidget::EditTargetsItem(const TargetData &newTarget) const
{
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

    targetsModel->update(newTarget.TargetId, newTarget);
}

void TargetsWidget::RemoveTargetsItem(const QStringList &targetsId) const
{
    QStringList filtered;
    for (auto targetId : targetsId) {
        for ( int i = 0; i < adaptixWidget->Targets.size(); i++ ) {
            if( adaptixWidget->Targets[i].TargetId == targetId ) {
                filtered.append(targetId);
                adaptixWidget->Targets.erase( adaptixWidget->Targets.begin() + i );
                break;
            }
        }
    }
    targetsModel->remove(filtered);
}

void TargetsWidget::TargetsSetTag(const QStringList &targetIds, const QString &tag) const
{
    QSet<QString> set1 = QSet<QString>(targetIds.begin(), targetIds.end());
    for ( int i = 0; i < adaptixWidget->Targets.size(); i++ ) {
        if( set1.contains(adaptixWidget->Targets[i].TargetId) ) {
            adaptixWidget->Targets[i].Tag = tag;
            set1.remove(adaptixWidget->Targets[i].TargetId);

            if (set1.size() == 0)
                break;
        }
    }

    targetsModel->setTag(targetIds, tag);
}

void TargetsWidget::UpdateColumnsSize() const
{
    tableView->resizeColumnToContents(TRC_Computer);
    tableView->resizeColumnToContents(TRC_Domain);
    tableView->resizeColumnToContents(TRC_Address);
    tableView->resizeColumnToContents(TRC_Os);
    tableView->resizeColumnToContents(TRC_Date);
}

void TargetsWidget::Clear() const
{
    adaptixWidget->Targets.clear();
    targetsModel->clear();
    inputFilter->clear();
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

    QString message = "";
    bool ok = false;
    bool result = HttpReqTargetsCreate(jsonData, *(adaptixWidget->GetProfile()), &message, &ok);
    if( !result ) {
        MessageError("Server is not responding");
        return;
    }
    if (!ok) MessageError(message);
}

/// Slots

void TargetsWidget::toggleSearchPanel() const
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

void TargetsWidget::onFilterUpdate() const
{
    proxyModel->setTextFilter(inputFilter->text());
}

void TargetsWidget::handleTargetsMenu(const QPoint &pos ) const
{
    QModelIndex index = tableView->indexAt(pos);
    if (!index.isValid()) return;

    QStringList targets;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString taskId = targetsModel->data(targetsModel->index(sourceIndex.row(), TRC_Id), Qt::DisplayRole).toString();
        targets.append(taskId);
    }

    auto ctxMenu = QMenu();

    int topCount = adaptixWidget->ScriptManager->AddMenuTargets(&ctxMenu, "TargetsTop", targets);
    if (topCount > 0)
        ctxMenu.addSeparator();

    ctxMenu.addAction("Create", this, &TargetsWidget::onCreateTarget );
    ctxMenu.addAction("Edit",   this, &TargetsWidget::onEditTarget );
    ctxMenu.addAction("Remove", this, &TargetsWidget::onRemoveTarget );
    ctxMenu.addSeparator();

    int centerCount = adaptixWidget->ScriptManager->AddMenuTargets(&ctxMenu, "TargetsCenter", targets);
    if (centerCount > 0)
        ctxMenu.addSeparator();

    ctxMenu.addAction("Set tag", this, &TargetsWidget::onSetTag );
    ctxMenu.addAction("Export",  this, &TargetsWidget::onExportTarget );
    int bottomCount = adaptixWidget->ScriptManager->AddMenuTargets(&ctxMenu, "TargetsBottom", targets);

    QPoint globalPos = tableView->mapToGlobal(pos);
    ctxMenu.exec(globalPos);
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

void TargetsWidget::onEditTarget() const
{
    auto idx = tableView->currentIndex();
    if (!idx.isValid()) return;

    QString targetId = proxyModel->index(idx.row(), TRC_Id).data().toString();

    bool found = false;
    TargetData targetData;
    for (auto target : adaptixWidget->Targets) {
        if (target.TargetId == targetId) {
            targetData = target;
            found = true;
            break;
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
    dataJson["t_target_id"] = newTargetData.TargetId;
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

    QString message = "";
    bool ok = false;
    bool result = HttpReqTargetEdit(jsonData, *(adaptixWidget->GetProfile()), &message, &ok);
    if( !result ) {
        MessageError("Server is not responding");
        return;
    }
    if (!ok) MessageError(message);
}

void TargetsWidget::onRemoveTarget() const
{
    QStringList listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString agentId = targetsModel->data(targetsModel->index(sourceIndex.row(), TRC_Id), Qt::DisplayRole).toString();
        listId.append(agentId);
    }

    if(listId.empty())
        return;

    QString message = QString();
    bool ok = false;
    HttpReqTargetRemove(listId, *(adaptixWidget->GetProfile()), &message, &ok);
}

void TargetsWidget::onSetTag() const
{
    QString tag = "";
    QStringList listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString cTag    = targetsModel->data(targetsModel->index(sourceIndex.row(), TRC_Tag), Qt::DisplayRole).toString();
        QString agentId = targetsModel->data(targetsModel->index(sourceIndex.row(), TRC_Id), Qt::DisplayRole).toString();
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
        bool result = HttpReqTargetSetTag(listId, newTag, *(adaptixWidget->GetProfile()), &message, &ok);
        if( !result ) {
            MessageError("Response timeout");
            return;
        }
    }
}

void TargetsWidget::onExportTarget() const
{
    auto idx = tableView->currentIndex();
    if (!idx.isValid()) return;

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

    NonBlockingDialogs::getSaveFileName(const_cast<TargetsWidget*>(this), "Save Targets", "targets.txt", "Text Files (*.txt);;All Files (*)",
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
