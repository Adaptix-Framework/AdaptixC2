#include <UI/Widgets/TargetsWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Dialogs/DialogTarget.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
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
            target.OsDesk.contains(filter1, Qt::CaseInsensitive) ||
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
    auto item_Os       = new QTableWidgetItem( target.OsDesk );
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

void TargetsWidget::SetData() const
{
    this->ClearTableContent();

    for (int i = 0; i < adaptixWidget->Targets.size(); i++ ) {
        if ( this->filterItem(adaptixWidget->Targets[i]) )
            this->addTableItem(adaptixWidget->Targets[i]);
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
        obj["os_desk"]  = target.OsDesk;
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
    auto ctxMenu = QMenu();

    ctxMenu.addAction("Create", this, &TargetsWidget::onCreateTarget );
    ctxMenu.addAction("Edit",   this, &TargetsWidget::onEditTarget );
    ctxMenu.addAction("Remove", this, &TargetsWidget::onRemoveTarget );
    ctxMenu.addSeparator();
    ctxMenu.addAction("Export", this, &TargetsWidget::onExportTarget );

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

}

void TargetsWidget::onRemoveTarget() const
{

}

void TargetsWidget::onExportTarget() const
{

}
