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
    connect(hideButton,  &ClickableLabel::clicked, this, &DownloadsWidget::toggleSearchPanel);
    connect(inputFilter, &QLineEdit::textChanged,  this, &DownloadsWidget::onFilterUpdate);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), tableView);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &DownloadsWidget::toggleSearchPanel);

    this->dockWidget->setWidget(this);
}

void DownloadsWidget::createUI()
{
    auto horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    searchWidget = new QWidget(this);
    searchWidget->setVisible(false);

    inputFilter = new QLineEdit(searchWidget);
    inputFilter->setPlaceholderText("filter by filename");
    inputFilter->setMaximumWidth(300);

    hideButton = new ClickableLabel("X");
    hideButton->setCursor(Qt::PointingHandCursor);

    searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(0, 5, 0, 0);
    searchLayout->setSpacing(4);
    searchLayout->addWidget(inputFilter);
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

    proxyModel->sort(-1);

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
    tableView->setUpdatesEnabled(enabled);
}

void DownloadsWidget::Clear() const
{
    adaptixWidget->Downloads.clear();
    downloadsModel->clear();
}

void DownloadsWidget::AddDownloadItem(const DownloadData &newDownload)
{
    if (adaptixWidget->Downloads.contains(newDownload.FileId))
        return;

    downloadsModel->add(newDownload);
    adaptixWidget->Downloads[newDownload.FileId] = newDownload;
}

void DownloadsWidget::EditDownloadItem(const QString &fileId, int recvSize, int state)
{
    adaptixWidget->Downloads[fileId].RecvSize = recvSize;
    adaptixWidget->Downloads[fileId].State = state;

    if (state == DOWNLOAD_STATE_FINISHED)
        adaptixWidget->Downloads[fileId].RecvSize = adaptixWidget->Downloads[fileId].TotalSize;

    if (state == DOWNLOAD_STATE_CANCELED) {
        adaptixWidget->Downloads.remove(fileId);
        downloadsModel->remove(fileId);
    } else {
        downloadsModel->update(fileId, recvSize, state);
    }
}

void DownloadsWidget::RemoveDownloadItem(const QString &fileId)
{
    adaptixWidget->Downloads.remove(fileId);
    downloadsModel->remove(fileId);
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
    proxyModel->setTextFilter(inputFilter->text());
    inputFilter->setFocus();
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
        int menuCount = adaptixWidget->ScriptManager->AddMenuDownload(&ctxMenu, "DownloadFinished", files);
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
        adaptixWidget->ScriptManager->AddMenuDownload(&ctxMenu, "DownloadRunning", files);
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

    QString command = QString("curl -k %1 -H 'OTP: %2' -o %3").arg(sUrl).arg(otp).arg(fileName);

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

    QString command = QString("wget --no-check-certificate %1 --header='OTP: %2' -O %3").arg(sUrl).arg(otp).arg(fileName);

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
    const DownloadData* download = getSelectedDownload();
    if (!download || download->State != DOWNLOAD_STATE_FINISHED)
        return;

    QString fileId = download->FileId;
    HttpReqDownloadActionAsync("delete", fileId, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}
