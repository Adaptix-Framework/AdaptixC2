#include <Agent/Agent.h>
#include <UI/Widgets/DownloadsWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <UI/Dialogs/DialogDownloader.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Utils/NonBlockingDialogs.h>
#include <Utils/CustomElements.h>

static QString extractFileName(const QString& filePath)
{
    QStringList pathParts = filePath.split("\\", Qt::SkipEmptyParts);
    QString fileName = pathParts.isEmpty() ? filePath : pathParts.last();
    pathParts = fileName.split("/", Qt::SkipEmptyParts);
    return pathParts.isEmpty() ? fileName : pathParts.last();
}

REGISTER_DOCK_WIDGET(DownloadsWidget, "Downloads", true)

DownloadsWidget::DownloadsWidget(AdaptixWidget* w) : DockTab("Downloads", w->GetProfile()->GetProject(), ":/icons/downloads"), adaptixWidget(w)
{
    this->createUI();

    connect(tableView,  &QTableView::customContextMenuRequested, this, &DownloadsWidget::handleDownloadsMenu);
    connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &selected, const QItemSelection &deselected){
        Q_UNUSED(selected)
        Q_UNUSED(deselected)
        tableView->setFocus();
    });
    connect(hideButton,     &ClickableLabel::clicked,        this, &DownloadsWidget::toggleSearchPanel);
    connect(inputFilter,     &QLineEdit::textChanged,        this, &DownloadsWidget::onFilterUpdate);
    connect(inputFilter,     &QLineEdit::returnPressed,      this, [this]() { proxyModel->setTextFilter(inputFilter->text()); });
    connect(stateComboBox,   &QComboBox::currentTextChanged, this, &DownloadsWidget::onStateFilterUpdate);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), this);
    shortcutSearch->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &DownloadsWidget::toggleSearchPanel);

    auto shortcutEsc = new QShortcut(QKeySequence(Qt::Key_Escape), inputFilter);
    shortcutEsc->setContext(Qt::WidgetShortcut);
    connect(shortcutEsc, &QShortcut::activated, this, [this]() { searchWidget->setVisible(false); });

    this->dockWidget->setWidget(this);
}

void DownloadsWidget::createUI()
{
    auto horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    searchWidget = new QWidget(this);
    searchWidget->setVisible(false);

    inputFilter = new QLineEdit(searchWidget);
    inputFilter->setPlaceholderText("filter: (exe | dll) & ^(temp)");
    inputFilter->setMaximumWidth(300);

    autoSearchCheck = new QCheckBox("auto", searchWidget);
    autoSearchCheck->setChecked(true);
    autoSearchCheck->setToolTip("Auto search on text change. If unchecked, press Enter to search.");

    stateComboBox = new QComboBox(searchWidget);
    stateComboBox->setMinimumWidth(100);
    stateComboBox->addItems(QStringList() << "Any state" << "Running" << "Stopped" << "Finished");

    hideButton = new ClickableLabel("  x  ");
    hideButton->setCursor(Qt::PointingHandCursor);
    hideButton->setStyleSheet("QLabel { color: #888; font-weight: bold; } QLabel:hover { color: #e34234; }");

    searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(0, 5, 0, 0);
    searchLayout->setSpacing(4);
    searchLayout->addWidget(inputFilter);
    searchLayout->addWidget(autoSearchCheck);
    searchLayout->addSpacing(8);
    searchLayout->addWidget(stateComboBox);
    searchLayout->addSpacing(8);
    searchLayout->addWidget(hideButton);
    searchLayout->addSpacerItem(horizontalSpacer);

    downloadsModel = new DownloadsTableModel(this);
    proxyModel = new DownloadsFilterProxyModel(this);
    proxyModel->setSourceModel(downloadsModel);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    tableView = new QTableView(this);
    tableView->setModel(proxyModel);
    tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    tableView->setAutoFillBackground(false);
    tableView->setShowGrid(false);
    tableView->setSortingEnabled(true);
    tableView->setWordWrap(true);
    tableView->setCornerButtonEnabled(false);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setFocusPolicy(Qt::NoFocus);
    tableView->setAlternatingRowColors(true);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setCascadingSectionResizes(true);
    tableView->horizontalHeader()->setHighlightSections(false);
    tableView->verticalHeader()->setVisible(false);

    tableView->setItemDelegate(new PaddingDelegate(tableView));

    tableView->sortByColumn(DC_Date, Qt::AscendingOrder);

    tableView->horizontalHeader()->setSectionResizeMode(DC_File, QHeaderView::Stretch);
    tableView->setItemDelegateForColumn(DC_Progress, new ProgressBarDelegate(this));
    tableView->hideColumn(DC_FileId);

    mainGridLayout = new QGridLayout(this);
    mainGridLayout->setContentsMargins(0, 0, 0, 0);
    mainGridLayout->addWidget(searchWidget, 0, 0, 1, 1);
    mainGridLayout->addWidget(tableView, 1, 0, 1, 1);
}

DownloadsWidget::~DownloadsWidget() = default;

void DownloadsWidget::SetUpdatesEnabled(bool enabled)
{
    if (!enabled) {
        bufferingEnabled = true;
    } else {
        bufferingEnabled = false;
        flushPendingDownloads();
    }

    if (proxyModel)
        proxyModel->setDynamicSortFilter(enabled);
    if (tableView)
        tableView->setSortingEnabled(enabled);

    tableView->setUpdatesEnabled(enabled);
}

void DownloadsWidget::flushPendingDownloads()
{
    if (pendingDownloads.isEmpty())
        return;

    QList<DownloadData> filtered;
    {
        QWriteLocker locker(&adaptixWidget->DownloadsLock);
        int count = 0;
        for (const auto& download : pendingDownloads) {
            if (adaptixWidget->Downloads.contains(download.FileId))
                continue;

            adaptixWidget->Downloads[download.FileId] = download;
            filtered.append(download);
        }
    }

    if (!filtered.isEmpty())
        downloadsModel->addBatch(filtered);

    pendingDownloads.clear();
}

void DownloadsWidget::Clear() const
{
    {
        QWriteLocker locker(&adaptixWidget->DownloadsLock);
        adaptixWidget->Downloads.clear();
    }
    downloadsModel->clear();
}

void DownloadsWidget::AddDownloadItem(const DownloadData &newDownload)
{
    if (bufferingEnabled) {
        pendingDownloads.append(newDownload);
        return;
    }

    QWriteLocker locker(&adaptixWidget->DownloadsLock);
    if (adaptixWidget->Downloads.contains(newDownload.FileId))
        return;

    adaptixWidget->Downloads[newDownload.FileId] = newDownload;
    locker.unlock();
    downloadsModel->add(newDownload);
}

void DownloadsWidget::EditDownloadItem(const QString &fileId, int recvSize, int state)
{
    {
        QWriteLocker locker(&adaptixWidget->DownloadsLock);
        if (!adaptixWidget->Downloads.contains(fileId))
            return;

        adaptixWidget->Downloads[fileId].RecvSize = recvSize;
        adaptixWidget->Downloads[fileId].State = state;

        if (state == DOWNLOAD_STATE_FINISHED)
            adaptixWidget->Downloads[fileId].RecvSize = adaptixWidget->Downloads[fileId].TotalSize;

        if (state == DOWNLOAD_STATE_CANCELED)
            adaptixWidget->Downloads.remove(fileId);
    }

    if (state == DOWNLOAD_STATE_CANCELED) {
        QStringList fileIds;
        fileIds.append(fileId);
        downloadsModel->remove(fileIds);
    } else {
        downloadsModel->update(fileId, recvSize, state);
    }
}

void DownloadsWidget::RemoveDownloadItem(const QStringList &filesId)
{
    QStringList filtered;
    {
        QWriteLocker locker(&adaptixWidget->DownloadsLock);
        for (auto fileId : filesId) {
            if (adaptixWidget->Downloads.contains(fileId)) {
                adaptixWidget->Downloads.remove(fileId);
                filtered.append(fileId);
            }
        }
    }
    downloadsModel->remove(filtered);
}

QString DownloadsWidget::getSelectedFileId() const
{
    QModelIndexList selected = tableView->selectionModel()->selectedRows();
    if (selected.isEmpty())
        return {};

    QModelIndex sourceIndex = proxyModel->mapToSource(selected.first());
    return downloadsModel->getFileIdAt(sourceIndex.row());
}

const DownloadData* DownloadsWidget::getSelectedDownload() const
{
    QString fileId = getSelectedFileId();
    if (fileId.isEmpty())
        return nullptr;
    return downloadsModel->getById(fileId);
}

/// SLOTS

void DownloadsWidget::toggleSearchPanel() const
{
    if (this->searchWidget->isVisible()) {
        this->searchWidget->setVisible(false);
        proxyModel->setSearchVisible(false);
    }
    else {
        this->searchWidget->setVisible(true);
        proxyModel->setSearchVisible(true);
        inputFilter->setFocus();
    }
}

void DownloadsWidget::onFilterUpdate() const
{
    if (autoSearchCheck->isChecked()) {
        proxyModel->setTextFilter(inputFilter->text());
    }
    inputFilter->setFocus();
    tableView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(DC_File, QHeaderView::Stretch);
}

void DownloadsWidget::onStateFilterUpdate() const
{
    int idx = stateComboBox->currentIndex();
    if (idx == 0)
        proxyModel->setStateFilter(-1);
    else
        proxyModel->setStateFilter(idx);
    tableView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(DC_File, QHeaderView::Stretch);
}

void DownloadsWidget::handleDownloadsMenu(const QPoint &pos)
{
    QModelIndex index = tableView->indexAt(pos);
    if (!index.isValid())
        return;

    const DownloadData* download = getSelectedDownload();
    if (!download)
        return;

    QVector<DataMenuDownload> files;
    DataMenuDownload data = {};
    data.agentId = download->AgentId;
    data.fileId  = download->FileId;
    data.path    = download->Filename;

    auto ctxMenu = QMenu();

    if (download->State == DOWNLOAD_STATE_FINISHED) {
        ctxMenu.addAction("Sync file to client", this, &DownloadsWidget::actionSync);

        auto syncMenu = new QMenu("Sync as ...", &ctxMenu);
        syncMenu->addAction("Curl command", this, &DownloadsWidget::actionSyncCurl);
        syncMenu->addAction("Wget command", this, &DownloadsWidget::actionSyncWget);
        ctxMenu.addMenu(syncMenu);
        ctxMenu.addSeparator();

        data.state = "finished";
        files.append(data);
        int menuCount = adaptixWidget->ScriptManager->AddMenuDownload(&ctxMenu, "DownloadFinished", files, false);
        if (menuCount > 0)
            ctxMenu.addSeparator();

        ctxMenu.addAction("Delete file", this, &DownloadsWidget::actionDelete);
    }
    else {
        if (download->State == DOWNLOAD_STATE_RUNNING) {
            data.state = "running";
        } else {
            data.state = "stopped";
        }
        files.append(data);
        adaptixWidget->ScriptManager->AddMenuDownload(&ctxMenu, "DownloadRunning", files, true);
    }

    ctxMenu.exec(tableView->viewport()->mapToGlobal(pos));
}

void DownloadsWidget::actionSync()
{
    const DownloadData* download = getSelectedDownload();
    if (!download || download->State != DOWNLOAD_STATE_FINISHED)
        return;

    QString fileId   = download->FileId;
    QString filePath = download->Filename;

    QString message = QString();
    bool ok = false;
    bool result = HttpReqGetOTP("download", fileId, *adaptixWidget->GetProfile(), &message, &ok);
    if (!result) {
        MessageError("Response timeout");
        return;
    }
    if (!ok) {
        MessageError(message);
        return;
    }
    QString otp = message;
    QString fileName = extractFileName(filePath);

    QString baseDir = fileName;
    if (adaptixWidget && adaptixWidget->GetProfile())
        baseDir = QDir(adaptixWidget->GetProfile()->GetProjectDir()).filePath(fileName);

    NonBlockingDialogs::getSaveFileName(this, "Save File", baseDir, "All Files (*.*)",
        [this, otp](const QString& savedPath) {
            if (savedPath.isEmpty())
                return;

            QString sUrl = adaptixWidget->GetProfile()->GetURL() + "/otp/download/sync";

            DialogDownloader dialog(sUrl, otp, savedPath);
            dialog.exec();
    });
}

void DownloadsWidget::actionSyncCurl()
{
    const DownloadData* download = getSelectedDownload();
    if (!download || download->State != DOWNLOAD_STATE_FINISHED)
        return;

    QString fileId   = download->FileId;
    QString filePath = download->Filename;

    QString message = QString();
    bool ok = false;
    bool result = HttpReqGetOTP("download", fileId, *adaptixWidget->GetProfile(), &message, &ok);
    if (!result) {
        MessageError("Response timeout");
        return;
    }
    if (!ok) {
        MessageError(message);
        return;
    }
    QString otp = message;
    QString fileName = extractFileName(filePath);
    QString sUrl = adaptixWidget->GetProfile()->GetURL() + "/otp/download/sync";

    QString command = QString("curl -k '%1?otp=%2' -o %3").arg(sUrl).arg(otp).arg(fileName);

    QInputDialog inputDialog;
    inputDialog.setWindowTitle("Sync file as curl");
    inputDialog.setLabelText("Curl command:");
    inputDialog.setTextEchoMode(QLineEdit::Normal);
    inputDialog.setTextValue(command);
    inputDialog.setFixedSize(700, 60);
    inputDialog.move(QGuiApplication::primaryScreen()->geometry().center() - inputDialog.geometry().center());
    inputDialog.exec();
}

void DownloadsWidget::actionSyncWget()
{
    const DownloadData* download = getSelectedDownload();
    if (!download || download->State != DOWNLOAD_STATE_FINISHED)
        return;

    QString fileId   = download->FileId;
    QString filePath = download->Filename;

    QString message = QString();
    bool ok = false;
    bool result = HttpReqGetOTP("download", fileId, *adaptixWidget->GetProfile(), &message, &ok);
    if (!result) {
        MessageError("Response timeout");
        return;
    }
    if (!ok) {
        MessageError(message);
        return;
    }
    QString otp = message;
    QString fileName = extractFileName(filePath);
    QString sUrl = adaptixWidget->GetProfile()->GetURL() + "/otp/download/sync";

    QString command = QString("wget --no-check-certificate '%1?otp=%2' -O %3").arg(sUrl).arg(otp).arg(fileName);

    QInputDialog inputDialog;
    inputDialog.setWindowTitle("Sync file as wget");
    inputDialog.setLabelText("Wget command:");
    inputDialog.setTextEchoMode(QLineEdit::Normal);
    inputDialog.setTextValue(command);
    inputDialog.setFixedSize(700, 60);
    inputDialog.move(QGuiApplication::primaryScreen()->geometry().center() - inputDialog.geometry().center());
    inputDialog.exec();
}

void DownloadsWidget::actionDelete()
{
    QStringList files;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) continue;

        QString fileId = downloadsModel->data(downloadsModel->index(sourceIndex.row(), DC_FileId), Qt::DisplayRole).toString();
        files.append(fileId);
    }

    for (auto fileId : files) {
        if ( downloadsModel->getById(fileId)->State != DOWNLOAD_STATE_FINISHED)
            files.removeAll(fileId);
    }
    if (files.isEmpty())
        return;

    HttpReqDownloadDelete(files, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}
