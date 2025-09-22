#include <Agent/Agent.h>
#include <Utils/FileSystem.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/BrowserFilesWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxScriptManager.h>

void BrowserFileData::CreateBrowserFileData(const QString &path, const int os)
{
    Fullpath = path;
    Type = TYPE_DIR;

    if (os == OS_WINDOWS) {
        Name = GetBasenameWindows(path);

        QString rootPath = GetRootPathWindows(path);
        if(rootPath == path)
            Type = TYPE_DISK;
    }
    else {
        Name = GetBasenameUnix(path);

        QString rootPath = GetRootPathUnix(path);
        if(rootPath == path)
            Type = TYPE_ROOTDIR;
    }

    TreeItem = new FileBrowserTreeItem(this);
    TreeItem->setIcon(0, GetFileSystemIcon(Type, false));
}

void BrowserFileData::SetType(const int type)
{
    this->Type = type;
    this->TreeItem->setIcon(0, GetFileSystemIcon(type, this->Stored));
}

void BrowserFileData::SetStored(const bool stored)
{
    this->Stored = stored;
    this->TreeItem->setIcon(0, GetFileSystemIcon(this->Type, this->Stored));
}




BrowserFilesWidget::BrowserFilesWidget(AdaptixWidget* w, Agent* a)
{
    agent = a;
    this->createUI();

    connect(buttonDisks,       &QPushButton::clicked,                     this, &BrowserFilesWidget::onDisks);
    connect(buttonList,        &QPushButton::clicked,                     this, &BrowserFilesWidget::onList);
    connect(buttonParent,      &QPushButton::clicked,                     this, &BrowserFilesWidget::onParent);
    connect(buttonReload,      &QPushButton::clicked,                     this, &BrowserFilesWidget::onReload);
    connect(buttonUpload,      &QPushButton::clicked,                     this, &BrowserFilesWidget::onUpload);
    connect(inputPath,         &QLineEdit::returnPressed,                 this, &BrowserFilesWidget::onList);
    connect(tableWidget,       &QTableWidget::doubleClicked,              this, &BrowserFilesWidget::handleTableDoubleClicked);
    connect(treeBrowserWidget, &QTreeWidget::itemDoubleClicked,           this, &BrowserFilesWidget::handleTreeDoubleClicked);
    connect(tableWidget,       &QTableWidget::customContextMenuRequested, this, &BrowserFilesWidget::handleTableMenu );
}

BrowserFilesWidget::~BrowserFilesWidget() = default;

void BrowserFilesWidget::createUI()
{
    buttonReload = new QPushButton(QIcon(":/icons/reload"), "", this);
    buttonReload->setIconSize( QSize( 24,24 ));
    buttonReload->setFixedSize(37, 28);
    buttonReload->setToolTip("Reload");

    buttonParent = new QPushButton(QIcon(":/icons/folder"), "", this);
    buttonParent->setIconSize(QSize(24, 24 ));
    buttonParent->setFixedSize(37, 28);
    buttonParent->setToolTip("Up folder");

    inputPath = new QLineEdit(this);

    buttonList = new QPushButton(QIcon(":/icons/arrow_right"), "", this);
    buttonList->setIconSize(QSize(24, 24 ));
    buttonList->setFixedSize(37, 28);

    line_1 = new QFrame(this);
    line_1->setFrameShape(QFrame::VLine);
    line_1->setMinimumHeight(25);

    buttonDisks = new QPushButton(QIcon(":/icons/storage"), "", this);
    buttonDisks->setIconSize( QSize( 24,24 ));
    buttonDisks->setFixedSize(37, 28);
    buttonDisks->setToolTip("Disks list");

    buttonUpload = new QPushButton(QIcon(":/icons/upload"), "", this);
    buttonUpload->setIconSize( QSize( 24,24 ));
    buttonUpload->setFixedSize(37, 28);
    buttonUpload->setToolTip("Upload File");

    line_2 = new QFrame(this);
    line_2->setFrameShape(QFrame::VLine);
    line_2->setMinimumHeight(25);

    statusLabel = new QLabel(this);
    statusLabel->setText("Status: ");

    tableWidget = new QTableWidget(this );
    tableWidget->setContextMenuPolicy( Qt::CustomContextMenu );
    tableWidget->setAutoFillBackground( false );
    tableWidget->setShowGrid( false );
    tableWidget->setSortingEnabled( false );
    tableWidget->setWordWrap( true );
    tableWidget->setCornerButtonEnabled( true );
    tableWidget->setSelectionBehavior( QAbstractItemView::SelectRows );
    tableWidget->setFocusPolicy( Qt::NoFocus );
    tableWidget->setAlternatingRowColors( true );
    tableWidget->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    tableWidget->horizontalHeader()->setCascadingSectionResizes( true );
    tableWidget->horizontalHeader()->setHighlightSections( false );
    tableWidget->verticalHeader()->setVisible( false );

    if (agent->data.Os == OS_WINDOWS) {
        tableWidget->setColumnCount(3);
        tableWidget->setHorizontalHeaderItem( 0, new QTableWidgetItem( "Name" ) );
        tableWidget->setHorizontalHeaderItem( 1, new QTableWidgetItem( "Size" ) );
        tableWidget->setHorizontalHeaderItem( 2, new QTableWidgetItem( "Last Modified" ) );
        tableWidget->setIconSize(QSize(25, 25));
    }
    else {
        tableWidget->setColumnCount(6);
        tableWidget->setHorizontalHeaderItem( 0, new QTableWidgetItem( "Name" ) );
        tableWidget->setHorizontalHeaderItem( 1, new QTableWidgetItem( "Mode" ) );
        tableWidget->setHorizontalHeaderItem( 2, new QTableWidgetItem( "User" ) );
        tableWidget->setHorizontalHeaderItem( 3, new QTableWidgetItem( "Group" ) );
        tableWidget->setHorizontalHeaderItem( 4, new QTableWidgetItem( "Size" ) );
        tableWidget->setHorizontalHeaderItem( 5, new QTableWidgetItem( "Last Modified" ) );
        tableWidget->setIconSize(QSize(25, 25));
    }

    listGridLayout = new QGridLayout(this);
    listGridLayout->setContentsMargins(5, 4, 1, 1);
    listGridLayout->setVerticalSpacing(4);
    listGridLayout->setHorizontalSpacing(8);

    listGridLayout->addWidget( buttonReload, 0, 0,  1, 1  );
    listGridLayout->addWidget( buttonParent, 0, 1,  1, 1  );
    listGridLayout->addWidget( inputPath,    0, 2,  1, 5  );
    listGridLayout->addWidget( buttonList,   0, 7,  1, 1  );
    listGridLayout->addWidget( line_1,       0, 8,  1, 1  );
    listGridLayout->addWidget( buttonUpload, 0, 9,  1, 1  );
    listGridLayout->addWidget( buttonDisks,  0, 10, 1, 1  );
    listGridLayout->addWidget( line_2,       0, 11, 1, 1  );
    listGridLayout->addWidget( statusLabel,  0, 12, 1, 1  );
    listGridLayout->addWidget( tableWidget,  1, 0,  1, 13 );

    listBrowserWidget = new QWidget(this);
    listBrowserWidget->setLayout(listGridLayout);

    treeBrowserWidget = new QTreeWidget();
    treeBrowserWidget->setSortingEnabled(false);
    treeBrowserWidget->headerItem()->setText( 0, "Directory Tree" );
    treeBrowserWidget->setIconSize(QSize(25, 25));

    splitter = new QSplitter( this );
    splitter->setOrientation( Qt::Horizontal );
    splitter->addWidget( treeBrowserWidget );
    splitter->addWidget( listBrowserWidget );
    splitter->setSizes( QList<int>() << 100 << 200 );

    mainGridLayout = new QGridLayout(this );
    mainGridLayout->setContentsMargins(0, 0, 0, 0 );
    mainGridLayout->addWidget(splitter, 0, 0, 1, 1 );

    this->setLayout(mainGridLayout);
}

void BrowserFilesWidget::SetDisksWin(const qint64 time, const int msgType, const QString &message, const QString &data)
{
    QString sTime  = UnixTimestampGlobalToStringLocal(time);
    QString status;
    if( msgType == CONSOLE_OUT_LOCAL_ERROR || msgType == CONSOLE_OUT_ERROR )
        status = TextColorHtml(message, COLOR_ChiliPepper) + " >> " + sTime;
    else
        status = TextColorHtml(message, COLOR_NeonGreen) + " >> " + sTime;

    statusLabel->setText(status);

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data.toUtf8());
    if (!jsonDoc.isArray())
        return;

    QVector<BrowserFileData*> disks;

    QJsonArray jsonArray = jsonDoc.array();
    for (const QJsonValue& value : jsonArray) {
        QJsonObject jsonObject = value.toObject();
        QString path = jsonObject["b_name"].toString();

        BrowserFileData* diskData = getFileData(path);
        diskData->Size = jsonObject["b_type"].toString();
        disks.push_back(diskData);
    }
    this->tableShowItems(disks);

    currentPath = "";
    inputPath->setText(currentPath);
}

void BrowserFilesWidget::AddFiles(const qint64 time, const int msgType, const QString &message, const QString &path, const QString &data)
{
    QString sTime  = UnixTimestampGlobalToStringLocal(time);
    QString status;
    if( msgType == CONSOLE_OUT_LOCAL_ERROR || msgType == CONSOLE_OUT_ERROR ) {
        status = TextColorHtml(message, COLOR_ChiliPepper) + " >> " + sTime;
        statusLabel->setText(status);
        return;
    }
    status = TextColorHtml(message, COLOR_NeonGreen) + " >> " + sTime;
    statusLabel->setText(status);

    QString fPath = "";
    if (this->agent->data.Os == OS_WINDOWS)
        fPath = path;
    else
        fPath = "/" + path;

    BrowserFileData* currentFileData = this->getFileData(fPath);
    currentFileData->SetStored(true);
    currentFileData->Status = status;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data.toUtf8());
    if (jsonDoc.isArray()) {
        QJsonArray jsonArray = jsonDoc.array();
        this->updateFileData(currentFileData, path, jsonArray);
    }

    this->tableShowItems(currentFileData->Files);

    treeBrowserWidget->setCurrentItem(currentFileData->TreeItem);
    currentFileData->TreeItem->setExpanded(true);

    currentPath = path;
    inputPath->setText(currentPath);
}

void BrowserFilesWidget::SetStatus(const qint64 time, const int msgType, const QString &message ) const
{
    QString sTime  = UnixTimestampGlobalToStringLocal(time);
    QString status;
    if( msgType == CONSOLE_OUT_LOCAL_ERROR || msgType == CONSOLE_OUT_ERROR ) {
        status = TextColorHtml(message, COLOR_ChiliPepper) + " >> " + sTime;
        statusLabel->setText(status);
    }
    else {
        status = TextColorHtml(message, COLOR_NeonGreen) + " >> " + sTime;
        statusLabel->setText(status);
    }
}

/// PRIVATE

BrowserFileData* BrowserFilesWidget::getBrowserStore(const QString &path) { return &browserStore[path]; }

void BrowserFilesWidget::setBrowserStore(const QString &path, const BrowserFileData &fileData) { browserStore[path] = fileData; }

BrowserFileData BrowserFilesWidget::createFileData(const QString &path) const
{
    BrowserFileData fileData;
    fileData.CreateBrowserFileData(path, this->agent->data.Os);
    return fileData;
}

BrowserFileData* BrowserFilesWidget::getFileData(const QString &path)
{
    QString fPath;
    if (this->agent->data.Os == OS_WINDOWS) {
        fPath = path.toLower();
    }
    else {
        if (path.startsWith("//"))
            fPath = path.mid(1);
        else
            fPath = path;
    }

    if(browserStore.contains(fPath))
        return this->getBrowserStore(fPath);

    BrowserFileData fileData = this->createFileData(path);
    this->setBrowserStore(fPath, fileData);

    QString parentPath;
    if (this->agent->data.Os == OS_WINDOWS) {
        if (fileData.Type == TYPE_DISK) {
            treeBrowserWidget->addTopLevelItem(fileData.TreeItem);
            return this->getBrowserStore(fPath);
        }
        parentPath = GetParentPathWindows(path);
    }
    else {
        if (fileData.Type == TYPE_ROOTDIR) {
            treeBrowserWidget->addTopLevelItem(fileData.TreeItem);
            return this->getBrowserStore(fPath);
        }
        parentPath = GetParentPathUnix(path);
    }

    BrowserFileData* parentFileData = this->getFileData(parentPath);
    parentFileData->TreeItem->addChild(fileData.TreeItem);

    return this->getBrowserStore(fPath);
}

void BrowserFilesWidget::updateFileData(BrowserFileData* currenFileData, const QString &path, QJsonArray jsonArray)
{
    QMap<QString, BrowserFileData> oldFiles;
    if (this->agent->data.Os == OS_WINDOWS) {
        for (BrowserFileData* oldData : currenFileData->Files)
            oldFiles[oldData->Name.toLower()] = *oldData;

        currenFileData->TreeItem->takeChildren();
        currenFileData->Files.clear();

        for ( QJsonValue value : jsonArray ) {
            QJsonObject jsonObject = value.toObject();
            QString filename = jsonObject["b_filename"].toString();
            qint64  b_size   = jsonObject["b_size"].toDouble();
            qint64  b_date   = jsonObject["b_date"].toDouble();
            int     b_type   = jsonObject["b_is_dir"].toBool();

            QString fullname = path + "\\" + filename;

            BrowserFileData* childData = this->getFileData(fullname);
            childData->Modified = UnixTimestampGlobalToStringLocalFull(b_date);

            if ( b_type == TYPE_FILE ) {
                childData->Size = BytesToFormat(b_size);
                childData->SetType(b_type);
            }

            if( oldFiles.contains(filename.toLower()) ) {
                oldFiles.remove(filename.toLower());
            }
            else {
                childData->SetType(b_type);
                if ( b_type != TYPE_FILE )
                    this->setBrowserStore(fullname, *childData);
            }

            currenFileData->TreeItem->addChild(childData->TreeItem);
            currenFileData->Files.push_back(childData);
        }

        for( QString oldPath : oldFiles.keys() ) {
            BrowserFileData data = oldFiles[oldPath];

            QString oldFullpath = data.Fullpath + "\\";

            for(QString storeKey : browserStore.keys())
                if (storeKey.startsWith(oldFullpath, Qt::CaseInsensitive))
                    browserStore.remove(storeKey);

            browserStore.remove(data.Fullpath);
            oldFiles.remove(oldPath);
        }
    }
    else {
        for (BrowserFileData* oldData : currenFileData->Files)
            oldFiles[oldData->Name] = *oldData;

        currenFileData->TreeItem->takeChildren();
        currenFileData->Files.clear();

        for ( QJsonValue value : jsonArray ) {
            QJsonObject jsonObject = value.toObject();
            int     b_type   = jsonObject["b_is_dir"].toBool();
            QString filename = jsonObject["b_filename"].toString();
            qint64  b_size   = jsonObject["b_size"].toDouble();
            QString b_date   = jsonObject["b_date"].toString();
            QString b_mode   = jsonObject["b_mode"].toString();
            QString b_user   = jsonObject["b_user"].toString();
            QString b_group  = jsonObject["b_group"].toString();

            QString fullname;
            if (path == "/")
                fullname = "//" + filename;
            else
                fullname = "/" + path + "/" + filename;

            BrowserFileData* childData = this->getFileData(fullname);
            childData->Modified = b_date;
            childData->User     = b_user;
            childData->Group    = b_group;
            childData->Mode     = b_mode;

            if ( b_type == TYPE_FILE ) {
                childData->Size = BytesToFormat(b_size);
                childData->SetType(b_type);
            }

            if( oldFiles.contains(filename) ) {
                oldFiles.remove(filename);
            }
            else {
                childData->SetType(b_type);
                if ( b_type != TYPE_FILE )
                    this->setBrowserStore(fullname, *childData);
            }

            currenFileData->TreeItem->addChild(childData->TreeItem);
            currenFileData->Files.push_back(childData);
        }

        for( QString oldPath : oldFiles.keys() ) {
            BrowserFileData data = oldFiles[oldPath];

            QString oldFullpath = data.Fullpath + "/";

            for(QString storeKey : browserStore.keys())
                if (storeKey.startsWith(oldFullpath, Qt::CaseSensitive))
                    browserStore.remove(storeKey);

            browserStore.remove(data.Fullpath);
            oldFiles.remove(oldPath);
        }
    }
}

void BrowserFilesWidget::setStoredFileData(const QString &path, const BrowserFileData &currenFileData)
{
    treeBrowserWidget->setCurrentItem(currenFileData.TreeItem);
    currenFileData.TreeItem->setExpanded(true);

    this->tableShowItems(currenFileData.Files);

    statusLabel->setText( currenFileData.Status );

    currentPath = path;
    inputPath->setText(currentPath);
}

void BrowserFilesWidget::tableShowItems(QVector<BrowserFileData*> files ) const
{
    for (int index = tableWidget->rowCount(); index > 0; index-- )
        tableWidget->removeRow(index -1 );

    tableWidget->setRowCount(files.size());
    for (int row = 0; row < files.size(); ++row) {

        QTableWidgetItem* item_Name = new QTableWidgetItem(files[row]->Name);
        item_Name->setIcon(GetFileSystemIcon( files[row]->Type, files[row]->Stored) );
        item_Name->setFlags( item_Name->flags() ^ Qt::ItemIsEditable );

        QTableWidgetItem* item_Size = new QTableWidgetItem(files[row]->Size);
        item_Size->setFlags( item_Size->flags() ^ Qt::ItemIsEditable );

        QTableWidgetItem* item_Date = new QTableWidgetItem(files[row]->Modified);
        item_Date->setFlags( item_Date->flags() ^ Qt::ItemIsEditable );

        if (this->agent->data.Os == OS_WINDOWS) {
            tableWidget->setItem(row, 0, item_Name);
            tableWidget->setItem(row, 1, item_Size);
            tableWidget->setItem(row, 2, item_Date);
        }
        else {
            QTableWidgetItem* item_Mode = new QTableWidgetItem(files[row]->Mode);
            item_Mode->setFlags( item_Mode->flags() ^ Qt::ItemIsEditable );

            QTableWidgetItem* item_User = new QTableWidgetItem(files[row]->User);
            item_User->setFlags( item_Mode->flags() ^ Qt::ItemIsEditable );

            QTableWidgetItem* item_Group = new QTableWidgetItem(files[row]->Group);
            item_Group->setFlags( item_Mode->flags() ^ Qt::ItemIsEditable );

            tableWidget->setItem(row, 0, item_Name);
            tableWidget->setItem(row, 1, item_Mode);
            tableWidget->setItem(row, 2, item_User);
            tableWidget->setItem(row, 3, item_Group);
            tableWidget->setItem(row, 4, item_Size);
            tableWidget->setItem(row, 5, item_Date);

        }
    }
}

void BrowserFilesWidget::cdBrowser(const QString &path)
{
    QString fPath;
    if ( this->agent->data.Os == OS_WINDOWS ) {
        fPath = path.toLower();
        if ( !browserStore.contains(fPath) )
            return;
    }
    else {
        fPath = path;
        if ( !browserStore.contains(fPath) )
            return;
    }

    BrowserFileData fileData = *this->getBrowserStore(fPath);
    if (fileData.Type == TYPE_FILE)
        return;

    if (fileData.Stored) {
        this->setStoredFileData(path, fileData);
    } else {
        statusLabel->setText("");
        Q_EMIT agent->adaptixWidget->eventFileBrowserList(agent->data.Id, path);
    }
}

/// SLOTS

void BrowserFilesWidget::onDisks() const
{
    statusLabel->setText("");
    Q_EMIT agent->adaptixWidget->eventFileBrowserDisks(agent->data.Id);
}

void BrowserFilesWidget::onList() const
{
    QString path = inputPath->text();
    statusLabel->setText("");
    Q_EMIT agent->adaptixWidget->eventFileBrowserList(agent->data.Id, path);
}

void BrowserFilesWidget::onParent()
{
    if (currentPath.isEmpty())
        return;

    QString path = "";

    if (this->agent->data.Os == OS_WINDOWS) {
        path = GetParentPathWindows(currentPath);
        if (path == currentPath)
            return;
    }
    else {
        path = GetParentPathUnix(currentPath);
        if (path == currentPath) {
            if (currentPath == "/")
                return;

            path = "/";
        }
    }

    this->cdBrowser(path);
}

void BrowserFilesWidget::onReload() const
{
    if ( currentPath.isEmpty() ) {
        QString path;
        if (this->agent->data.Os == OS_WINDOWS)
            path = ".\\";
        else
            path = "./";

        statusLabel->setText("");
        Q_EMIT agent->adaptixWidget->eventFileBrowserList(agent->data.Id, path);
    }
    else {
        statusLabel->setText("");
        Q_EMIT agent->adaptixWidget->eventFileBrowserList(agent->data.Id, currentPath);
    }
}

void BrowserFilesWidget::onUpload() const
{
    QString path = inputPath->text();
    if ( path.isEmpty() )
        return;

    QString remotePath = currentPath;
    if (this->agent->data.Os == OS_WINDOWS)
        remotePath += "\\";
    else
        remotePath += "/";

    QString filePath = QFileDialog::getOpenFileName(nullptr, "Select file", QDir::homePath());
    if ( filePath.isEmpty())
        return;

    statusLabel->setText("");
    emit agent->adaptixWidget->eventFileBrowserUpload(agent->data.Id, remotePath, filePath);
}

void BrowserFilesWidget::handleTableDoubleClicked(const QModelIndex &index)
{
    QString filename = tableWidget->item(index.row(),0)->text();

    QString path = filename;
    if ( !currentPath.isEmpty() ) {
        if (this->agent->data.Os == OS_WINDOWS)
            path = currentPath + "\\" + filename;
        else
            path = currentPath + "/" + filename;
    }

    this->cdBrowser(path);
}

void BrowserFilesWidget::handleTreeDoubleClicked(QTreeWidgetItem* item, int column)
{
    FileBrowserTreeItem* treeItem = static_cast<FileBrowserTreeItem *>(item);

    QString path = treeItem->Data.Fullpath;
    if (path == currentPath )
        return;

    this->cdBrowser(path);
}

void BrowserFilesWidget::handleTableMenu(const QPoint &pos)
{
    if ( !tableWidget->itemAt(pos) || currentPath.isEmpty())
        return;

    if ( !(agent && agent->adaptixWidget && agent->adaptixWidget->ScriptManager) )
        return;

    QString path = currentPath;
    if (this->agent->data.Os == OS_WINDOWS) {
        if (!path.endsWith("\\"))
            path += "\\";
    }
    else {
        if (!path.endsWith("/"))
            path += "/";
    }

    QVector<DataMenuFileBrowser> items;
    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( tableWidget->item(rowIndex, 0)->isSelected() ) {

            auto filename = tableWidget->item( rowIndex, 0 )->text();
            auto fullname = path + filename;
            if (this->agent->data.Os == OS_WINDOWS)
                fullname = fullname.toLower();

            if (!browserStore.contains(fullname))
                continue;

            DataMenuFileBrowser dataFile = {};
            dataFile.agentId = agent->data.Id;
            dataFile.path    = path;

            int filetype = this->getBrowserStore(fullname)->Type;
            if (filetype == TYPE_FILE)
                items.append(DataMenuFileBrowser{agent->data.Id, path, filename, "file"});
            else
                items.append(DataMenuFileBrowser{agent->data.Id, path, filename, "dir"});
        }
    }

    auto ctxMenu = QMenu();

    agent->adaptixWidget->ScriptManager->AddMenuFileBrowser(&ctxMenu, items);

    ctxMenu.exec(tableWidget->horizontalHeader()->viewport()->mapToGlobal(pos));
}