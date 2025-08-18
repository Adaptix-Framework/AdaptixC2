#include <UI/Widgets/TargetsWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Dialogs/DialogTarget.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Utils/CustomElements.h>


TargetsWidget::TargetsWidget(AdaptixWidget* w) : adaptixWidget(w)
{
    this->createUI();

     connect(tableWidget,  &QTableWidget::customContextMenuRequested, this, &TargetsWidget::handleTargetsMenu);
     connect(tableWidget,  &QTableWidget::cellDoubleClicked,          this, &TargetsWidget::onEditTarget);
     connect(tableWidget,  &QTableWidget::itemSelectionChanged,       this, [this](){tableWidget->setFocus();} );
     connect(hideButton,   &ClickableLabel::clicked,                  this, &TargetsWidget::toggleSearchPanel);
     connect(inputFilter,  &QLineEdit::textChanged,                   this, &TargetsWidget::onFilterUpdate);

     shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), tableWidget);
     shortcutSearch->setContext(Qt::WidgetShortcut);
     connect(shortcutSearch, &QShortcut::activated, this, &TargetsWidget::toggleSearchPanel);
}

TargetsWidget::~TargetsWidget() = default;

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

    tableWidget = new QTableWidget( this );
    tableWidget->setColumnCount( 8 );
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

    tableWidget->setHorizontalHeaderItem(ColumnId,       new QTableWidgetItem("Id"));
    tableWidget->setHorizontalHeaderItem(ColumnComputer, new QTableWidgetItem("Computer"));
    tableWidget->setHorizontalHeaderItem(ColumnDomain,   new QTableWidgetItem("Domain"));
    tableWidget->setHorizontalHeaderItem(ColumnAddress,  new QTableWidgetItem("Address"));
    tableWidget->setHorizontalHeaderItem(ColumnTag,      new QTableWidgetItem("Tag"));
    tableWidget->setHorizontalHeaderItem(ColumnOs,       new QTableWidgetItem("OS"));
    tableWidget->setHorizontalHeaderItem(ColumnDate,     new QTableWidgetItem("Date"));
    tableWidget->setHorizontalHeaderItem(ColumnInfo,     new QTableWidgetItem("Info"));

    tableWidget->hideColumn(0);

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->setContentsMargins( 0, 0,  0, 0);
    mainGridLayout->addWidget( searchWidget, 0, 0, 1, 1);
    mainGridLayout->addWidget( tableWidget,  1, 0, 1, 1);
}

bool TargetsWidget::filterItem(const TargetData &target) const
{
    if ( !this->searchWidget->isVisible() )
        return true;

    QString filter1 = this->inputFilter->text();
    if( !filter1.isEmpty() ) {
        if ( target.Computer.contains(filter1, Qt::CaseInsensitive) ||
            target.Domain.contains(filter1, Qt::CaseInsensitive) ||
            target.Address.contains(filter1, Qt::CaseInsensitive) ||
            target.Tag.contains(filter1, Qt::CaseInsensitive) ||
            target.OsDesc.contains(filter1, Qt::CaseInsensitive) ||
            target.Info.contains(filter1, Qt::CaseInsensitive)
        )
            return true;
        else
            return false;
    }
    return true;
}

void TargetsWidget::addTableItem(const TargetData &target) const
{
    auto item_TargetId = new QTableWidgetItem( target.TargetId );
    auto item_Computer = new QTableWidgetItem( target.Computer );
    auto item_Domain   = new QTableWidgetItem( target.Domain );
    auto item_Address  = new QTableWidgetItem( target.Address );
    auto item_Tag      = new QTableWidgetItem( target.Tag );
    auto item_Os       = new QTableWidgetItem( target.OsDesc );
    auto item_Date     = new QTableWidgetItem( target.Date );
    auto item_Info     = new QTableWidgetItem( target.Info );

    item_Computer->setFlags( item_Computer->flags() ^ Qt::ItemIsEditable );
    item_Computer->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );

    item_Domain->setFlags( item_Domain->flags() ^ Qt::ItemIsEditable );
    item_Domain->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );

    item_Address->setFlags( item_Address->flags() ^ Qt::ItemIsEditable );
    item_Address->setTextAlignment( Qt::AlignCenter );

    item_Os->setFlags( item_Os->flags() ^ Qt::ItemIsEditable );
    item_Os->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );

    item_Tag->setFlags( item_Tag->flags() ^ Qt::ItemIsEditable );
    item_Tag->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );

    item_Date->setFlags( item_Date->flags() ^ Qt::ItemIsEditable );
    item_Date->setTextAlignment( Qt::AlignCenter );

    item_Info->setFlags( item_Info->flags() ^ Qt::ItemIsEditable );
    item_Info->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );

    if (target.Os == OS_WINDOWS) {
        if (target.Owned) {
            item_Os->setIcon(QIcon(":/icons/os_win_red"));
        } else if(target.Alive) {
            item_Os->setIcon(QIcon(":/icons/os_win_blue"));
        } else {
            item_Os->setIcon(QIcon(":/icons/os_win_grey"));
        }
    }
    else if (target.Os == OS_LINUX) {
        if (target.Owned) {
            item_Os->setIcon(QIcon(":/icons/os_linux_red"));
        } else if(target.Alive) {
            item_Os->setIcon(QIcon(":/icons/os_linux_blue"));
        } else {
            item_Os->setIcon(QIcon(":/icons/os_linux_grey"));
        }
    }
    else if (target.Os == OS_MAC) {
        if (target.Owned) {
            item_Os->setIcon(QIcon(":/icons/os_mac_red"));
        } else if(target.Alive) {
            item_Os->setIcon(QIcon(":/icons/os_mac_blue"));
        } else {
            item_Os->setIcon(QIcon(":/icons/os_mac_grey"));
        }
    }

    if( tableWidget->rowCount() < 1 )
        tableWidget->setRowCount( 1 );
    else
        tableWidget->setRowCount( tableWidget->rowCount() + 1 );

    bool isSortingEnabled = tableWidget->isSortingEnabled();
    tableWidget->setSortingEnabled( false );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnId,       item_TargetId );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnComputer, item_Computer );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnDomain,   item_Domain );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnAddress,  item_Address );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnTag,      item_Tag );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnOs,       item_Os );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnDate,     item_Date );
    tableWidget->setItem( tableWidget->rowCount() - 1, ColumnInfo,     item_Info );
    tableWidget->setSortingEnabled( isSortingEnabled );

    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnComputer, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnDomain,   QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnAddress,  QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnOs,       QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( ColumnDate,     QHeaderView::ResizeToContents );

    tableWidget->verticalHeader()->setSectionResizeMode(tableWidget->rowCount() - 1, QHeaderView::ResizeToContents);
}

/// Main

void TargetsWidget::Clear() const
{
    adaptixWidget->Targets.clear();
    this->ClearTableContent();
    inputFilter->clear();
}

void TargetsWidget::AddTargetsItems(QList<TargetData> targetList) const
{
    for (auto target : targetList) {
        for( auto t : adaptixWidget->Targets ) {
            if( t.TargetId == target.TargetId )
                continue;
        }

        adaptixWidget->Targets.push_back(target);

        if( !this->filterItem(target) )
            continue;

        this->addTableItem(target);
    }
}

void TargetsWidget::EditTargetsItem(const TargetData &newTarget) const
{
    for ( int i = 0; i < adaptixWidget->Targets.size(); i++ ) {
        if( adaptixWidget->Targets[i].TargetId == newTarget.TargetId ) {
            adaptixWidget->Targets[i].Computer = newTarget.Computer;
            adaptixWidget->Targets[i].Domain   = newTarget.Domain;
            adaptixWidget->Targets[i].Address  = newTarget.Address;
            adaptixWidget->Targets[i].Tag      = newTarget.Tag;
            adaptixWidget->Targets[i].Os       = newTarget.Os;
            adaptixWidget->Targets[i].OsDesc   = newTarget.OsDesc;
            adaptixWidget->Targets[i].Date     = newTarget.Date;
            adaptixWidget->Targets[i].Info     = newTarget.Info;
            adaptixWidget->Targets[i].Alive    = newTarget.Alive;
            adaptixWidget->Targets[i].Owned    = newTarget.Owned;
            break;
        }
    }

    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, ColumnId);
        if ( item && item->text() == newTarget.TargetId ) {
            tableWidget->item(row, ColumnComputer)->setText(newTarget.Computer);
            tableWidget->item(row, ColumnDomain  )->setText(newTarget.Domain);
            tableWidget->item(row, ColumnAddress )->setText(newTarget.Address);
            tableWidget->item(row, ColumnTag     )->setText(newTarget.Tag);
            tableWidget->item(row, ColumnOs      )->setText(newTarget.OsDesc);
            tableWidget->item(row, ColumnDate    )->setText(newTarget.Date);
            tableWidget->item(row, ColumnInfo    )->setText(newTarget.Info);

            if (newTarget.Os == OS_WINDOWS) {
                if (newTarget.Owned) {
                    tableWidget->item(row, ColumnOs)->setIcon(QIcon(":/icons/os_win_red"));
                } else if(newTarget.Alive) {
                    tableWidget->item(row, ColumnOs)->setIcon(QIcon(":/icons/os_win_blue"));
                } else {
                    tableWidget->item(row, ColumnOs)->setIcon(QIcon(":/icons/os_win_grey"));
                }
            }
            else if (newTarget.Os == OS_LINUX) {
                if (newTarget.Owned) {
                    tableWidget->item(row, ColumnOs)->setIcon(QIcon(":/icons/os_linux_red"));
                } else if(newTarget.Alive) {
                    tableWidget->item(row, ColumnOs)->setIcon(QIcon(":/icons/os_linux_blue"));
                } else {
                    tableWidget->item(row, ColumnOs)->setIcon(QIcon(":/icons/os_linux_grey"));
                }
            }
            else if (newTarget.Os == OS_MAC) {
                if (newTarget.Owned) {
                    tableWidget->item(row, ColumnOs)->setIcon(QIcon(":/icons/os_mac_red"));
                } else if(newTarget.Alive) {
                    tableWidget->item(row, ColumnOs)->setIcon(QIcon(":/icons/os_mac_blue"));
                } else {
                    tableWidget->item(row, ColumnOs)->setIcon(QIcon(":/icons/os_mac_grey"));
                }
            }
        }
    }
}

void TargetsWidget::SetData() const
{
    this->ClearTableContent();

    for (int i = 0; i < adaptixWidget->Targets.size(); i++ ) {
        if ( this->filterItem(adaptixWidget->Targets[i]) )
            this->addTableItem(adaptixWidget->Targets[i]);
    }
}

void TargetsWidget::RemoveTargetsItem(const QString &targetId) const
{
    for ( int i = 0; i < adaptixWidget->Targets.size(); i++ ) {
        if( adaptixWidget->Targets[i].TargetId == targetId ) {
            adaptixWidget->Targets.erase( adaptixWidget->Targets.begin() + i );
            break;
        }
    }

    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, ColumnId);
        if ( item && item->text() == targetId ) {
            tableWidget->removeRow(row);
            break;
        }
    }
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

    QSet<QString> set2 = QSet<QString>(targetIds.begin(), targetIds.end());
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

void TargetsWidget::ClearTableContent() const
{
    for (int row = tableWidget->rowCount() - 1; row >= 0; row--) {
        for (int col = 0; col < tableWidget->columnCount(); ++col)
            tableWidget->takeItem(row, col);

        tableWidget->removeRow(row);
    }
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
    if (this->searchWidget->isVisible())
        this->searchWidget->setVisible(false);
    else
        this->searchWidget->setVisible(true);

    this->SetData();
}

void TargetsWidget::onFilterUpdate() const { this->SetData(); }

void TargetsWidget::handleTargetsMenu(const QPoint &pos ) const
{
    QStringList targets;
    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( tableWidget->item(rowIndex, 0)->isSelected() ) {
            QString targetId = tableWidget->item( rowIndex, ColumnId )->text();
            targets.append(targetId);
        }
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

    QPoint globalPos = tableWidget->mapToGlobal(pos);
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
    if (tableWidget->selectionModel()->selectedRows().empty())
        return;

    auto targetId = tableWidget->item( tableWidget->currentRow(), ColumnId )->text();

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
    if (tableWidget->selectionModel()->selectedRows().empty())
        return;

    auto targetId = tableWidget->item( tableWidget->currentRow(), ColumnId )->text();

    QString message = QString();
    bool ok = false;
    bool result = HttpReqTargetRemove(targetId, *(adaptixWidget->GetProfile()), &message, &ok);
    if( !result ){
        MessageError("Response timeout");
        return;
    }

    if ( !ok ) MessageError(message);
}

void TargetsWidget::onSetTag() const
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
        bool result = HttpReqTargetSetTag(listId, newTag, *(adaptixWidget->GetProfile()), &message, &ok);
        if( !result ) {
            MessageError("Response timeout");
            return;
        }
    }
}

void TargetsWidget::onExportTarget() const
{
    if (tableWidget->selectionModel()->selectedRows().empty())
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

    QString fileName = QFileDialog::getSaveFileName( nullptr, "Save Targets", "targets.txt", "Text Files (*.txt);;All Files (*)" );
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

             QString computer = tableWidget->item(rowIndex, ColumnComputer)->text();
             QString domain   = tableWidget->item(rowIndex, ColumnDomain)->text();
             QString address  = tableWidget->item(rowIndex, ColumnAddress)->text();

             QString temp = format;
             content += temp
             .replace("%computer%", computer)
             .replace("%domain%", domain)
             .replace("%address%", address)
             + "\n";
         }
     }

     file.write(content.trimmed().toUtf8());
     file.close();
}
