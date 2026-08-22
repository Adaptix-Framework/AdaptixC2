#include <Agent/Agent.h>
#include <Utils/FileSystem.h>
#include <Utils/NonBlockingDialogs.h>
#include <Utils/CustomElements/BoldHeaderView.h>
#include <Utils/CustomElements/Delegates.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/BrowserFilesWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxScriptManager.h>

#include <oclero/qlementine/widgets/Menu.hpp>
#include <oclero/qlementine/utils/ImageUtils.hpp>

#include <QApplication>
#include <QClipboard>

#include <algorithm>

REGISTER_DOCK_WIDGET(BrowserFilesWidget, "Browser Files", false)

void BrowserFileData::CreateBrowserFileData(const QString& path, const int os)
{
    Fullpath = path;
    Type = TYPE_DIR;

    if (os == OS_WINDOWS) {
        Name = GetBasenameWindows(path);

        const QString rootPath = GetRootPathWindows(path);
        if (rootPath == path)
            Type = TYPE_DISK;
    } else {
        Name = GetBasenameUnix(path);

        const QString rootPath = GetRootPathUnix(path);
        if (rootPath == path)
            Type = TYPE_ROOTDIR;
    }

    TreeItem = new FileBrowserTreeItem(this);
    TreeItem->setIcon(0, GetFileSystemIcon(Type, false));
}

void BrowserFileData::SetType(const int type)
{
    this->Type = type;
    if (this->TreeItem)
        this->TreeItem->setIcon(0, GetFileSystemIcon(type, this->Stored));
}

void BrowserFileData::SetStored(const bool stored)
{
    this->Stored = stored;
    if (this->TreeItem)
        this->TreeItem->setIcon(0, GetFileSystemIcon(this->Type, this->Stored));
}




BrowserFilesWidget::BrowserFilesWidget(const AdaptixWidget* w, Agent* a) : DockTab(QString("Files [%1]").arg(a->data.Id), w->GetProfile()->GetProject(), QString(), const_cast<AdaptixWidget*>(w))
{
    agent = a;
    this->createUI();

    connect(buttonDisks,  &QPushButton::clicked,             this, &BrowserFilesWidget::onDisks);
    connect(buttonList,   &QPushButton::clicked,             this, &BrowserFilesWidget::onList);
    connect(buttonParent, &QPushButton::clicked,             this, &BrowserFilesWidget::onParent);
    connect(buttonReload, &QPushButton::clicked,             this, &BrowserFilesWidget::onReload);
    connect(buttonUpload, &QPushButton::clicked,             this, &BrowserFilesWidget::onUpload);
    connect(inputPath,    &QLineEdit::returnPressed,         this, &BrowserFilesWidget::onList);
    connect(tableView,    &QTableView::doubleClicked,        this, &BrowserFilesWidget::handleTableDoubleClicked);
    connect(treeBrowserWidget, &QTreeWidget::itemDoubleClicked, this, &BrowserFilesWidget::handleTreeDoubleClicked);
    connect(tableView,    &QTableView::customContextMenuRequested, this, &BrowserFilesWidget::handleTableMenu);
    connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection&, const QItemSelection&) { tableView->setFocus(); });

    this->dockWidget->setWidget(this);
}

BrowserFilesWidget::~BrowserFilesWidget()
{
    clearBrowserStore();
}

void BrowserFilesWidget::clearAgent()
{
    stopLoading();
    agent = nullptr;
}

void BrowserFilesWidget::createUI()
{
    buttonReload = new QPushButton(QIcon(":/icons/reload"), "", this);
    buttonReload->setIconSize(QSize(24, 24));
    buttonReload->setFixedSize(37, 28);
    buttonReload->setToolTip("Reload");

    buttonParent = new QPushButton(QIcon(":/icons/folder"), "", this);
    buttonParent->setIconSize(QSize(24, 24));
    buttonParent->setFixedSize(37, 28);
    buttonParent->setToolTip("Up folder");

    inputPath = new QLineEdit(this);

    buttonList = new QPushButton(QIcon(":/icons/arrow_right"), "", this);
    buttonList->setIconSize(QSize(24, 24));
    buttonList->setFixedSize(37, 28);

    line_1 = new QFrame(this);
    line_1->setFrameShape(QFrame::VLine);
    line_1->setMinimumHeight(25);

    buttonDisks = new QPushButton(QIcon(":/icons/storage"), "", this);
    buttonDisks->setIconSize(QSize(24, 24));
    buttonDisks->setFixedSize(37, 28);
    buttonDisks->setToolTip("Disks list");

    buttonUpload = new QPushButton(QIcon(":/icons/upload"), "", this);
    buttonUpload->setIconSize(QSize(24, 24));
    buttonUpload->setFixedSize(37, 28);
    buttonUpload->setToolTip("Upload File");

    line_2 = new QFrame(this);
    line_2->setFrameShape(QFrame::VLine);
    line_2->setMinimumHeight(25);

    statusLabel = new QLabel(this);
    statusLabel->setText("Status: ");

    loadingSpinner = new oclero::qlementine::LoadingSpinner(this);
    loadingSpinner->setFixedSize(16, 16);
    loadingSpinner->setVisible(false);

    tableModel = new QStandardItemModel(this);

    tableView = new QTableView(this);
    tableView->setModel(tableModel);
    tableView->setHorizontalHeader(new BoldHeaderView(Qt::Horizontal, tableView));
    tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    tableView->setAutoFillBackground(false);
    tableView->setShowGrid(false);
    tableView->setSortingEnabled(false);
    tableView->setWordWrap(true);
    tableView->setCornerButtonEnabled(true);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setFocusPolicy(Qt::NoFocus);
    tableView->setAlternatingRowColors(true);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableView->horizontalHeader()->setCascadingSectionResizes(true);
    tableView->horizontalHeader()->setHighlightSections(false);
    tableView->verticalHeader()->setVisible(false);
    tableView->setItemDelegate(new PaddingDelegate(tableView));

    if (agent && agent->data.Os == OS_WINDOWS) {
        tableModel->setColumnCount(3);
        tableModel->setHorizontalHeaderItem(0, new QStandardItem("Name"));
        tableModel->setHorizontalHeaderItem(1, new QStandardItem("Size"));
        tableModel->setHorizontalHeaderItem(2, new QStandardItem("Last Modified"));
    } else {
        tableModel->setColumnCount(6);
        tableModel->setHorizontalHeaderItem(0, new QStandardItem("Name"));
        tableModel->setHorizontalHeaderItem(1, new QStandardItem("Mode"));
        tableModel->setHorizontalHeaderItem(2, new QStandardItem("User"));
        tableModel->setHorizontalHeaderItem(3, new QStandardItem("Group"));
        tableModel->setHorizontalHeaderItem(4, new QStandardItem("Size"));
        tableModel->setHorizontalHeaderItem(5, new QStandardItem("Last Modified"));
    }
    tableView->setIconSize(QSize(25, 25));
    tableView->setProperty("autoIconColor", QVariant::fromValue(oclero::qlementine::AutoIconColor::None));

    listGridLayout = new QGridLayout(this);
    listGridLayout->setContentsMargins(5, 4, 1, 1);
    listGridLayout->setVerticalSpacing(4);
    listGridLayout->setHorizontalSpacing(8);

    listGridLayout->addWidget(buttonReload,    0, 0,  1, 1);
    listGridLayout->addWidget(buttonParent,    0, 1,  1, 1);
    listGridLayout->addWidget(inputPath,       0, 2,  1, 5);
    listGridLayout->addWidget(buttonList,      0, 7,  1, 1);
    listGridLayout->addWidget(line_1,          0, 8,  1, 1);
    listGridLayout->addWidget(buttonUpload,    0, 9,  1, 1);
    listGridLayout->addWidget(buttonDisks,     0, 10, 1, 1);
    listGridLayout->addWidget(line_2,          0, 11, 1, 1);
    listGridLayout->addWidget(loadingSpinner,  0, 12, 1, 1);
    listGridLayout->addWidget(statusLabel,     0, 13, 1, 1);
    listGridLayout->addWidget(tableView,       1, 0,  1, 14);

    listBrowserWidget = new QWidget(this);
    listBrowserWidget->setLayout(listGridLayout);

    treeBrowserWidget = new QTreeWidget();
    treeBrowserWidget->setSortingEnabled(false);
    treeBrowserWidget->setExpandsOnDoubleClick(false);
    treeBrowserWidget->headerItem()->setText(0, "Directory Tree");
    treeBrowserWidget->setIconSize(QSize(25, 25));
    treeBrowserWidget->setProperty("autoIconColor", QVariant::fromValue(oclero::qlementine::AutoIconColor::None));

    splitter = new QSplitter(this);
    splitter->setOrientation(Qt::Horizontal);
    splitter->addWidget(treeBrowserWidget);
    splitter->addWidget(listBrowserWidget);
    splitter->setSizes(QList<int>() << 100 << 200);

    mainGridLayout = new QGridLayout(this);
    mainGridLayout->setContentsMargins(0, 0, 0, 0);
    mainGridLayout->addWidget(splitter, 0, 0, 1, 1);

    this->setLayout(mainGridLayout);
}

void BrowserFilesWidget::stopLoading() const
{
    if (loadingSpinner) {
        loadingSpinner->setSpinning(false);
        loadingSpinner->setVisible(false);
    }
}

bool BrowserFilesWidget::isWindows() const
{
    return agent && agent->data.Os == OS_WINDOWS;
}

QString BrowserFilesWidget::normalizePath(const QString& path) const
{
    if (path.isEmpty() || !agent)
        return path;

    if (isWindows()) {
        QString p = path;
        p.replace(QLatin1Char('/'), QLatin1Char('\\'));
        if (p.startsWith(QLatin1String("\\\\"))) {
        } else {
            while (p.contains(QLatin1String("\\\\")))
                p.replace(QLatin1String("\\\\"), QLatin1String("\\"));
        }
        return p.toLower();
    }

    QString p = path;
    while (p.startsWith(QLatin1String("//")))
        p = p.mid(1);
    if (!p.startsWith(QLatin1Char('/')))
        p.prepend(QLatin1Char('/'));
    while (p.contains(QLatin1String("//")))
        p.replace(QLatin1String("//"), QLatin1String("/"));
    return p;
}

QString BrowserFilesWidget::joinPath(const QString& dir, const QString& name) const
{
    if (isWindows()) {
        QString d = dir;
        while (d.endsWith(QLatin1Char('\\')) && !(d.length() == 3 && d[1] == QLatin1Char(':')))
            d.chop(1);
        if (d.isEmpty())
            return normalizePath(name);
        if (d.length() == 2 && d[1] == QLatin1Char(':'))
            return normalizePath(d + QLatin1Char('\\') + name);
        return normalizePath(d + QLatin1Char('\\') + name);
    }

    if (dir == QLatin1String("/") || dir.isEmpty())
        return normalizePath(QLatin1Char('/') + name);
    return normalizePath(dir + QLatin1Char('/') + name);
}

BrowserFileData* BrowserFilesWidget::getBrowserStore(const QString& path) const
{
    return browserStore.value(path, nullptr);
}

void BrowserFilesWidget::removeStoreEntry(const QString& path)
{
    BrowserFileData* data = browserStore.take(path);
    if (!data)
        return;

    if (data->TreeItem) {
        QTreeWidgetItem* item = data->TreeItem;
        if (QTreeWidgetItem* parent = item->parent()) {
            parent->removeChild(item);
        } else if (treeBrowserWidget) {
            const int idx = treeBrowserWidget->indexOfTopLevelItem(item);
            if (idx >= 0)
                treeBrowserWidget->takeTopLevelItem(idx);
        }
        while (item->childCount() > 0)
            item->takeChild(0);

        delete item;
        data->TreeItem = nullptr;
    }

    data->Files.clear();
    delete data;
}

void BrowserFilesWidget::removeStoreSubtree(const QString& fullpath)
{
    if (fullpath.isEmpty())
        return;

    const QString sep = isWindows() ? QStringLiteral("\\") : QStringLiteral("/");
    const Qt::CaseSensitivity cs = isWindows() ? Qt::CaseInsensitive : Qt::CaseSensitive;

    QStringList keysToRemove;
    for (auto it = browserStore.constBegin(); it != browserStore.constEnd(); ++it) {
        const QString& key = it.key();
        if (key.compare(fullpath, cs) == 0 || key.startsWith(fullpath + sep, cs))
            keysToRemove.append(key);
    }

    std::sort(keysToRemove.begin(), keysToRemove.end(), [](const QString& a, const QString& b) {
        return a.size() > b.size();
    });

    for (const QString& key : keysToRemove)
        removeStoreEntry(key);
}

void BrowserFilesWidget::clearBrowserStore()
{
    const QStringList keys = browserStore.keys();
    for (const QString& key : keys)
        removeStoreEntry(key);
    browserStore.clear();
    currentPath.clear();
}

void BrowserFilesWidget::SetDisksWin(const qint64 time, const int msgType, const QString& message, const QString& data)
{
    if (!agent)
        return;

    stopLoading();

    const QString sTime = UnixTimestampGlobalToStringLocal(time);
    QString status;
    if (msgType == CONSOLE_OUT_LOCAL_ERROR || msgType == CONSOLE_OUT_ERROR)
        status = TextColorHtml(message, COLOR_ChiliPepper) + " >> " + sTime;
    else
        status = TextColorHtml(message, COLOR_NeonGreen) + " >> " + sTime;
    statusLabel->setText(status);

    const QJsonDocument jsonDoc = QJsonDocument::fromJson(data.toUtf8());
    if (!jsonDoc.isArray())
        return;

    QVector<BrowserFileData*> disks;
    const QJsonArray jsonArray = jsonDoc.array();
    for (const QJsonValue& value : jsonArray) {
        const QJsonObject jsonObject = value.toObject();
        const QString path = jsonObject["b_name"].toString();
        if (path.isEmpty())
            continue;

        BrowserFileData* diskData = getFileData(path);
        if (!diskData)
            continue;
        diskData->Size = jsonObject["b_type"].toString();
        disks.push_back(diskData);
    }

    tableShowItems(disks);

    currentPath.clear();
    inputPath->setText(currentPath);
}

void BrowserFilesWidget::AddFiles(const qint64 time, const int msgType, const QString& message, const QString& path, const QString& data)
{
    if (!agent)
        return;

    stopLoading();

    const QString sTime = UnixTimestampGlobalToStringLocal(time);
    QString status;
    if (msgType == CONSOLE_OUT_LOCAL_ERROR || msgType == CONSOLE_OUT_ERROR) {
        status = TextColorHtml(message, COLOR_ChiliPepper) + " >> " + sTime;
        statusLabel->setText(status);
        return;
    }
    status = TextColorHtml(message, COLOR_NeonGreen) + " >> " + sTime;
    statusLabel->setText(status);

    const QString fPath = normalizePath(path);
    if (fPath.isEmpty())
        return;

    BrowserFileData* currentFileData = getFileData(fPath);
    if (!currentFileData)
        return;

    currentFileData->SetStored(true);
    currentFileData->Status = status;

    const QJsonDocument jsonDoc = QJsonDocument::fromJson(data.toUtf8());
    if (jsonDoc.isArray())
        updateFileData(currentFileData, fPath, jsonDoc.array());

    tableShowItems(currentFileData->Files);

    if (currentFileData->TreeItem) {
        treeBrowserWidget->setCurrentItem(currentFileData->TreeItem);
        currentFileData->TreeItem->setExpanded(true);
    }

    currentPath = fPath;
    inputPath->setText(currentPath);
}

void BrowserFilesWidget::SetStatus(const qint64 time, const int msgType, const QString& message) const
{
    stopLoading();

    if (!statusLabel)
        return;

    const QString sTime = UnixTimestampGlobalToStringLocal(time);
    QString status;
    if (msgType == CONSOLE_OUT_LOCAL_ERROR || msgType == CONSOLE_OUT_ERROR)
        status = TextColorHtml(message, COLOR_ChiliPepper) + " >> " + sTime;
    else
        status = TextColorHtml(message, COLOR_NeonGreen) + " >> " + sTime;
    statusLabel->setText(status);
}

/// PRIVATE

BrowserFileData* BrowserFilesWidget::getFileData(const QString& path)
{
    if (!agent)
        return nullptr;

    const QString fPath = normalizePath(path);
    if (fPath.isEmpty())
        return nullptr;

    if (BrowserFileData* existing = getBrowserStore(fPath))
        return existing;

    auto* fileData = new BrowserFileData();
    fileData->CreateBrowserFileData(fPath, agent->data.Os);
    browserStore.insert(fPath, fileData);

    if (fileData->Type == TYPE_DISK || fileData->Type == TYPE_ROOTDIR) {
        treeBrowserWidget->addTopLevelItem(fileData->TreeItem);
        return fileData;
    }

    const QString parentPath = isWindows() ? GetParentPathWindows(fPath) : GetParentPathUnix(fPath);
    BrowserFileData* parentFileData = getFileData(parentPath);
    if (parentFileData && parentFileData->TreeItem && fileData->TreeItem)
        parentFileData->TreeItem->addChild(fileData->TreeItem);

    return fileData;
}

void BrowserFilesWidget::updateFileData(BrowserFileData* currentFileData, const QString& path, const QJsonArray& jsonArray)
{
    if (!agent || !currentFileData)
        return;

    const QString npath = normalizePath(path);
    const bool win = isWindows();

    QMap<QString, BrowserFileData*> oldByName;
    for (BrowserFileData* oldData : currentFileData->Files) {
        if (!oldData)
            continue;
        const QString key = win ? oldData->Name.toLower() : oldData->Name;
        oldByName.insert(key, oldData);
    }

    if (currentFileData->TreeItem)
        currentFileData->TreeItem->takeChildren();
    currentFileData->Files.clear();

    for (const QJsonValue& value : jsonArray) {
        const QJsonObject jsonObject = value.toObject();
        const QString filename = jsonObject["b_filename"].toString();
        if (filename.isEmpty() || filename == QLatin1String(".") || filename == QLatin1String(".."))
            continue;

        const bool isDir = jsonObject["b_is_dir"].toBool();
        const int b_type = isDir ? TYPE_DIR : TYPE_FILE;
        const qint64 b_size = jsonObject["b_size"].toVariant().toLongLong();

        const QString fullname = joinPath(npath, filename);
        BrowserFileData* childData = getFileData(fullname);
        if (!childData)
            continue;

        if (win) {
            const qint64 b_date = jsonObject["b_date"].toVariant().toLongLong();
            childData->Modified = UnixTimestampGlobalToStringLocalFull(b_date);
        } else {
            childData->Modified = jsonObject["b_date"].toString();
            childData->User     = jsonObject["b_user"].toString();
            childData->Group    = jsonObject["b_group"].toString();
            childData->Mode     = jsonObject["b_mode"].toString();
        }

        if (b_type == TYPE_FILE) {
            childData->Size = BytesToFormat(b_size);
            childData->SetType(TYPE_FILE);
        } else {
            childData->Size.clear();
            if (childData->Type != TYPE_DISK && childData->Type != TYPE_ROOTDIR)
                childData->SetType(TYPE_DIR);
        }

        const QString nameKey = win ? filename.toLower() : filename;
        oldByName.remove(nameKey);

        if (currentFileData->TreeItem && childData->TreeItem)
            currentFileData->TreeItem->addChild(childData->TreeItem);
        currentFileData->Files.push_back(childData);
    }

    for (auto it = oldByName.constBegin(); it != oldByName.constEnd(); ++it) {
        BrowserFileData* gone = it.value();
        if (!gone)
            continue;
        removeStoreSubtree(gone->Fullpath);
    }
}

void BrowserFilesWidget::setStoredFileData(const QString& path, BrowserFileData* fileData)
{
    if (!fileData)
        return;

    if (fileData->TreeItem) {
        treeBrowserWidget->setCurrentItem(fileData->TreeItem);
        fileData->TreeItem->setExpanded(true);
    }

    tableShowItems(fileData->Files);
    statusLabel->setText(fileData->Status);

    currentPath = path;
    inputPath->setText(currentPath);
}

void BrowserFilesWidget::tableShowItems(const QVector<BrowserFileData*>& files) const
{
    if (!tableModel)
        return;

    tableModel->removeRows(0, tableModel->rowCount());
    tableModel->setRowCount(files.size());

    const bool win = isWindows();

    for (int row = 0; row < files.size(); ++row) {
        BrowserFileData* f = files[row];
        if (!f)
            continue;

        auto* item_Name = new QStandardItem(f->Name);
        item_Name->setIcon(GetFileSystemIcon(f->Type, f->Stored));
        item_Name->setFlags(item_Name->flags() & ~Qt::ItemIsEditable);

        auto* item_Size = new QStandardItem(f->Size);
        item_Size->setFlags(item_Size->flags() & ~Qt::ItemIsEditable);

        auto* item_Date = new QStandardItem(f->Modified);
        item_Date->setFlags(item_Date->flags() & ~Qt::ItemIsEditable);

        if (win) {
            tableModel->setItem(row, 0, item_Name);
            tableModel->setItem(row, 1, item_Size);
            tableModel->setItem(row, 2, item_Date);
        } else {
            auto* item_Mode = new QStandardItem(f->Mode);
            item_Mode->setFlags(item_Mode->flags() & ~Qt::ItemIsEditable);

            auto* item_User = new QStandardItem(f->User);
            item_User->setFlags(item_User->flags() & ~Qt::ItemIsEditable);

            auto* item_Group = new QStandardItem(f->Group);
            item_Group->setFlags(item_Group->flags() & ~Qt::ItemIsEditable);

            tableModel->setItem(row, 0, item_Name);
            tableModel->setItem(row, 1, item_Mode);
            tableModel->setItem(row, 2, item_User);
            tableModel->setItem(row, 3, item_Group);
            tableModel->setItem(row, 4, item_Size);
            tableModel->setItem(row, 5, item_Date);
        }
    }
}

void BrowserFilesWidget::cdBrowser(const QString& path)
{
    if (!agent)
        return;

    const QString fPath = normalizePath(path);
    BrowserFileData* fileData = getBrowserStore(fPath);
    if (!fileData)
        return;

    if (fileData->Type == TYPE_FILE)
        return;

    if (fileData->Stored) {
        setStoredFileData(fPath, fileData);
    } else {
        statusLabel->setText("");
        loadingSpinner->setVisible(true);
        loadingSpinner->setSpinning(true);
        Q_EMIT agent->adaptixWidget->eventFileBrowserList(agent->data.Id, fPath);
    }
}

/// SLOTS

void BrowserFilesWidget::onDisks() const
{
    if (!agent || !agent->adaptixWidget)
        return;

    statusLabel->setText("");
    loadingSpinner->setVisible(true);
    loadingSpinner->setSpinning(true);
    Q_EMIT agent->adaptixWidget->eventFileBrowserDisks(agent->data.Id);
}

void BrowserFilesWidget::onList() const
{
    if (!agent || !agent->adaptixWidget)
        return;

    const QString path = inputPath->text().trimmed();
    statusLabel->setText("");
    loadingSpinner->setVisible(true);
    loadingSpinner->setSpinning(true);
    Q_EMIT agent->adaptixWidget->eventFileBrowserList(agent->data.Id, path);
}

void BrowserFilesWidget::onParent()
{
    if (!agent)
        return;

    if (currentPath.isEmpty())
        return;

    QString path;
    if (isWindows()) {
        path = GetParentPathWindows(currentPath);
        if (path == currentPath)
            return;
    } else {
        path = GetParentPathUnix(currentPath);
        if (path == currentPath) {
            if (currentPath == QLatin1String("/"))
                return;
            path = QStringLiteral("/");
        }
    }

    cdBrowser(path);
}

void BrowserFilesWidget::onReload() const
{
    if (!agent || !agent->adaptixWidget)
        return;

    QString path = currentPath;
    if (path.isEmpty())
        path = isWindows() ? QStringLiteral(".\\") : QStringLiteral("./");

    statusLabel->setText("");
    loadingSpinner->setVisible(true);
    loadingSpinner->setSpinning(true);
    Q_EMIT agent->adaptixWidget->eventFileBrowserList(agent->data.Id, path);
}

void BrowserFilesWidget::onUpload() const
{
    if (!agent || !agent->adaptixWidget)
        return;

    if (currentPath.isEmpty())
        return;

    QString remotePath = currentPath;
    if (isWindows()) {
        if (!remotePath.endsWith(QLatin1Char('\\')))
            remotePath += QLatin1Char('\\');
    } else {
        if (!remotePath.endsWith(QLatin1Char('/')))
            remotePath += QLatin1Char('/');
    }

    QString baseDir = QDir::homePath();
    if (agent->adaptixWidget->GetProfile())
        baseDir = agent->adaptixWidget->GetProfile()->GetProjectDir();

    NonBlockingDialogs::getOpenFileName(const_cast<BrowserFilesWidget*>(this), "Select file", baseDir, "All Files (*.*)",
        [this, remotePath](const QString& filePath) {
            if (filePath.isEmpty() || !agent || !agent->adaptixWidget)
                return;

            statusLabel->setText("");
            loadingSpinner->setVisible(true);
            loadingSpinner->setSpinning(true);
            Q_EMIT agent->adaptixWidget->eventFileBrowserUpload(agent->data.Id, remotePath, filePath);
        });
}

void BrowserFilesWidget::handleTableDoubleClicked(const QModelIndex& index)
{
    if (!agent || !index.isValid())
        return;

    QStandardItem* nameItem = tableModel->item(index.row(), 0);
    if (!nameItem)
        return;

    const QString filename = nameItem->text();
    QString path = filename;
    if (!currentPath.isEmpty())
        path = joinPath(currentPath, filename);

    cdBrowser(path);
}

void BrowserFilesWidget::handleTreeDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    if (!agent || !item)
        return;

    auto* treeItem = static_cast<FileBrowserTreeItem*>(item);
    const QString path = treeItem->Fullpath;
    if (normalizePath(path) == currentPath)
        return;

    cdBrowser(path);
}

void BrowserFilesWidget::handleTableMenu(const QPoint& pos)
{
    if (!agent || !tableView || !tableModel)
        return;

    if (!tableView->indexAt(pos).isValid() || currentPath.isEmpty())
        return;

    QString path = currentPath;
    if (isWindows()) {
        if (!path.endsWith(QLatin1Char('\\')))
            path += QLatin1Char('\\');
    } else {
        if (!path.endsWith(QLatin1Char('/')))
            path += QLatin1Char('/');
    }

    QVector<DataMenuFileBrowser> items;
    QStringList fullPaths;
    for (int rowIndex = 0; rowIndex < tableModel->rowCount(); rowIndex++) {
        if (!tableView->selectionModel()->isSelected(tableModel->index(rowIndex, 0)))
            continue;

        QStandardItem* nameItem = tableModel->item(rowIndex, 0);
        if (!nameItem)
            continue;

        const QString filename = nameItem->text();
        const QString fullname = joinPath(currentPath, filename);

        BrowserFileData* entry = getBrowserStore(fullname);
        if (!entry)
            continue;

        const QString absPath = entry->Fullpath.isEmpty() ? fullname : entry->Fullpath;
        if (!absPath.isEmpty())
            fullPaths.append(absPath);

        if (entry->Type == TYPE_FILE)
            items.append(DataMenuFileBrowser{agent->data.Id, path, filename, QStringLiteral("file")});
        else
            items.append(DataMenuFileBrowser{agent->data.Id, path, filename, QStringLiteral("dir")});
    }

    if (items.isEmpty() && fullPaths.isEmpty())
        return;

    oclero::qlementine::Menu ctxMenu;

    if (!fullPaths.isEmpty()) {
        const QString label = fullPaths.size() == 1 ? QStringLiteral("Copy full path") : QStringLiteral("Copy full paths (%1)").arg(fullPaths.size());
        ctxMenu.addAction(QIcon(QStringLiteral(":/icons/copy_all")), label, this, [this, fullPaths]() {
            if (QClipboard* cb = QApplication::clipboard())
                cb->setText(fullPaths.join(QLatin1Char('\n')));
            if (statusLabel) {
                if (fullPaths.size() == 1)
                    statusLabel->setText(QStringLiteral("Copied path to clipboard"));
                else
                    statusLabel->setText(QStringLiteral("Copied %1 paths to clipboard").arg(fullPaths.size()));
            }
        });
        ctxMenu.addSeparator();
    }

    if (agent->adaptixWidget && agent->adaptixWidget->ScriptManager && !items.isEmpty())
        agent->adaptixWidget->ScriptManager->AddMenuFileBrowser(&ctxMenu, items);

    if (ctxMenu.actions().isEmpty())
        return;

    ctxMenu.exec(tableView->viewport()->mapToGlobal(pos));
}