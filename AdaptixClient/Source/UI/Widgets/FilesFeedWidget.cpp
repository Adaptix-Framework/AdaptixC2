#include <UI/Widgets/FilesFeedWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/Settings.h>
#include <Utils/CustomElements/ListFeed.h>
#include <Utils/NonBlockingDialogs.h>
#include <UI/Dialogs/DialogDownloader.h>
#include <Utils/FontManager.h>
#include <Client/AxScript/AxScriptManager.h>
#include <MainAdaptix.h>

#include <oclero/qlementine/widgets/Menu.hpp>
#include <oclero/qlementine/widgets/SegmentedControl.hpp>

#include <QDesktopServices>
#include <QProcess>
#include <QUrl>
#include <QClipboard>
#include <QFileInfo>

#include <algorithm>

REGISTER_DOCK_WIDGET(FilesFeedWidget, "Files Feed", true)

namespace FF { enum { FileId = 0, Badge, Created, Name, Path, User, Computer, AgentId, Tags, Size, Status, Count }; }

static bool filesCol(int col)
{
    return GlobalClient && GlobalClient->settings && GlobalClient->settings->data.FilesTableColumns[col];
}

static QString formatFileSize(qint64 bytes)
{
    if (bytes < 1024)
        return QString("%1 b").arg(bytes);
    if (bytes < 1024*1024)
        return QString("%1 Kb").arg(bytes/1024.0, 0, 'f', 1);
    if (bytes < 1024*1024*1024)
        return QString("%1 Mb").arg(bytes/(1024.0*1024), 0, 'f', 1);
    return
        QString("%1 Gb").arg(bytes/(1024.0*1024*1024), 0, 'f', 1);
}

static QString extractFileName(const QString& filePath)
{
    QStringList pathParts = filePath.split("\\", Qt::SkipEmptyParts);
    QString fileName = pathParts.isEmpty() ? filePath : pathParts.last();
    pathParts = fileName.split("/", Qt::SkipEmptyParts);
    return pathParts.isEmpty() ? fileName : pathParts.last();
}

static FeedRow transferToFeedRow(const TransferData& t, bool isDownload)
{
    FeedRow row;
    row.resize(FFB_Count);
    row.entityId = t.FileId;

    QString badge;
    if (!isDownload && filesCol(FF::Badge))
        badge = (t.Kind == TRANSFER_KIND_MEMORY) ? "memory" : "file";

    row[FFB_Id] = QVariantMap{
        {"id", filesCol(FF::FileId) ? QString("#%1").arg(t.FileId) : QString()},
        {"badge", badge},
        {"date", filesCol(FF::Created) ? QDateTime::fromSecsSinceEpoch(t.DateTimestamp).toString("dd/MM HH:mm:ss") : QString()}
    };

    QString fullPath = t.Filename;
    QString baseName = fullPath;
    int sp = fullPath.lastIndexOf('/');
    int bp = fullPath.lastIndexOf('\\');
    if (bp > sp) sp = bp;
    if (sp >= 0) baseName = fullPath.mid(sp + 1);

    row[FFB_Main] = QVariantMap{
        {"main", filesCol(FF::Name) ? baseName : QString()},
        {"submain", QString()},
        {"second", filesCol(FF::Path) ? fullPath : QString()}
    };

    QString infoMain;
    if (filesCol(FF::User) && filesCol(FF::Computer) && !t.User.isEmpty() && !t.Computer.isEmpty())
        infoMain = QString("%1 @ %2").arg(t.User, t.Computer);
    else if (filesCol(FF::User) && !t.User.isEmpty())
        infoMain = t.User;
    else if (filesCol(FF::Computer) && !t.Computer.isEmpty())
        infoMain = t.Computer;

    row[FFB_Info] = QVariantMap{
        {"main", infoMain},
        {"second", (filesCol(FF::AgentId) && t.AgentId > 0) ? QString("agent: %1").arg(t.AgentId) : QString()},
        {"computer", t.Computer}
    };

    QStringList tags;
    if (filesCol(FF::Tags) && !t.Tag.isEmpty()) tags << t.Tag;
    row[FFB_Tags] = QVariant(tags);

    QString status;
    QString statusType;
    if (filesCol(FF::Status)) {
        switch (t.State) {
            case TRANSFER_STATE_RUNNING:  status = "Running";  statusType = "info";    break;
            case TRANSFER_STATE_STOPPED:  status = "Stopped";  statusType = "warning"; break;
            case TRANSFER_STATE_FINISHED: status = "Finished"; statusType = "success"; break;
            case TRANSFER_STATE_CANCELED: status = "Canceled"; statusType = "muted";   break;
            default:                      status = "Unknown";  statusType = "muted";
        }
    }

    QString statusMain;
    QString statusProgress;
    QString statusSecond;
    if (filesCol(FF::Size)) {
        if (t.State == TRANSFER_STATE_FINISHED) {
            statusMain = formatFileSize(t.TotalSize);
        } else if (t.State == TRANSFER_STATE_RUNNING && t.TotalSize > 0) {
            double pct = (double)t.Progress / t.TotalSize * 100.0;
            statusProgress = QString("%1%").arg(qRound(pct));
            qint64 remaining = t.TotalSize - t.Progress;
            if (remaining < 0) remaining = 0;
            statusSecond = QString("%1 / %2").arg(formatFileSize(t.Progress), formatFileSize(remaining));
        }
    }

    row[FFB_Right] = QVariantMap{
        {"main", statusMain},
        {"second", statusSecond},
        {"status", status},
        {"statusType", statusType},
        {"progress", statusProgress}
    };

    row.isDead = (t.State == TRANSFER_STATE_CANCELED || t.State == TRANSFER_STATE_STOPPED);
    return row;
}

static FeedRow syncToFeedRow(const SyncEntryData& s)
{
    FeedRow row;
    row.resize(FFB_Count);
    row.entityId = 0;

    QString dir = filesCol(FF::Badge)
        ? ((s.direction == TRANSFER_DOWNLOAD) ? QString::fromUtf8("\u2193") : QString::fromUtf8("\u2191"))
        : QString();
    row[FFB_Id] = QVariantMap{
        {"id", filesCol(FF::FileId) ? s.id.left(8) : QString()},
        {"badge", dir},
        {"date", (filesCol(FF::Created) && s.timestamp > 0)
            ? QDateTime::fromSecsSinceEpoch(s.timestamp).toString("dd/MM HH:mm:ss") : QString()},
        {"syncId", s.id}
    };

    row[FFB_Main] = QVariantMap{
        {"main", filesCol(FF::Name) ? s.filename : QString()},
        {"submain", QString()},
        {"second", filesCol(FF::Path) ? s.localPath : QString()}
    };

    row[FFB_Info] = QVariantMap{{"main", QString()}, {"second", QString()}, {"computer", QString()}};
    row[FFB_Tags] = QVariant(QStringList{});

    QString status;
    QString statusType;
    if (filesCol(FF::Status)) {
        switch (s.state) {
            case TRANSFER_STATE_RUNNING:  status = "Running";  statusType = "info";    break;
            case TRANSFER_STATE_STOPPED:  status = "Stopped";  statusType = "warning"; break;
            case TRANSFER_STATE_FINISHED: status = "Finished"; statusType = "success"; break;
            case TRANSFER_STATE_CANCELED: status = "Canceled"; statusType = "muted";   break;
            default:                      status = "Unknown";  statusType = "muted";
        }
    }

    QString statusMain;
    QString statusProgress;
    QString statusSecond;
    if (filesCol(FF::Size)) {
        if (s.state == TRANSFER_STATE_FINISHED) {
            statusMain = formatFileSize(s.totalSize);
        } else if (s.state == TRANSFER_STATE_RUNNING && s.totalSize > 0) {
            double pct = (double)s.progress / s.totalSize * 100.0;
            statusProgress = QString("%1%").arg(qRound(pct));
            qint64 remaining = s.totalSize - s.progress;
            if (remaining < 0) remaining = 0;
            statusSecond = QString("%1 / %2").arg(formatFileSize(s.progress), formatFileSize(remaining));
        }
    }

    row[FFB_Right] = QVariantMap{
        {"main", statusMain},
        {"second", statusSecond},
        {"status", status},
        {"statusType", statusType},
        {"progress", statusProgress},
        {"totalSize", s.totalSize}
    };
    row.isDead = (s.state == TRANSFER_STATE_CANCELED || s.state == TRANSFER_STATE_STOPPED);
    return row;
}

static ListFeedDelegate* createFilesDelegate(QObject* parent)
{
    auto* d = new ListFeedDelegate(parent);
    d->addBlock(new IdBadgeBlock());
    d->addBlock(new MainBlock());
    d->addBlock(new TextBlock());
    d->addBlock(new TagsBlock());
    d->addBlock(new StatusBlock());
    return d;
}

static ListFeedWidget* createFeedWidget(AdaptixWidget* w, FeedListModel* model, QWidget* parent)
{
    auto* feed = new ListFeedWidget(w);
    feed->setModel(model);
    auto* delegate = createFilesDelegate(feed);
    delegate->setFeedModel(model);
    feed->setDelegate(delegate);
    feed->enableSearch(true);
    feed->enableFilterCombo(true, "All computers");
    feed->enablePagination(true);
    feed->finalizeSearchWidget();
    feed->enableCompactSwitch(true);
    if (GlobalClient && GlobalClient->settings)
        feed->setCompactMode(GlobalClient->settings->data.FilesCompactMode);
    feed->setBlockGap(12);
    feed->rebuildModelChain();

    return feed;
}

static ListFeedWidget* createSyncFeedWidget(AdaptixWidget* w, FeedListModel* model, QWidget* parent)
{
    auto* feed = new ListFeedWidget(w);
    feed->setModel(model);
    auto* delegate = createFilesDelegate(feed);
    delegate->setFeedModel(model);
    feed->setDelegate(delegate);
    feed->enableSearch(true);
    feed->finalizeSearchWidget();
    feed->enableCompactSwitch(true);
    if (GlobalClient && GlobalClient->settings)
        feed->setCompactMode(GlobalClient->settings->data.FilesCompactMode);
    feed->setBlockGap(12);
    feed->rebuildModelChain();

    auto* pbar = feed->paginationBar();
    if (pbar) {
        pbar->blockSignals(true);
        auto* spin = pbar->findChild<QSpinBox*>();
        if (spin)
            spin->setValue(100);
        pbar->blockSignals(false);
    }

    return feed;
}

FilesFeedWidget::FilesFeedWidget(AdaptixWidget* w) : QWidget(w), m_adaptixWidget(w)
{
    dlModel = new FeedListModel(this);
    ulModel = new FeedListModel(this);
    syncModel = new FeedListModel(this);

    dlFeed = createFeedWidget(w, dlModel, this);
    ulFeed = createFeedWidget(w, ulModel, this);
    syncFeed = createSyncFeedWidget(w, syncModel, this);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(dlFeed);
    m_stack->addWidget(ulFeed);
    m_stack->addWidget(syncFeed);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_stack, 1);

    auto makeSegment = [this](ListFeedWidget* feed, int idx) {
        auto* seg = new oclero::qlementine::SegmentedControl(feed);
        seg->addItem("Downloads");
        seg->addItem("Uploads");
        seg->addItem("Sync");
        seg->setCurrentIndex(idx);
        seg->setFixedHeight(FontManager::instance().typography().segmentHeight);
        feed->addToolbarWidgetBefore(seg);
        return seg;
    };

    m_segDl   = makeSegment(dlFeed, 0);
    m_segUl   = makeSegment(ulFeed, 1);
    m_segSync = makeSegment(syncFeed, 2);

    auto onSegmentChanged = [this](int idx) {
        m_currentSegment = idx;
        m_stack->setCurrentIndex(idx);
        if (m_segDl)   { m_segDl->blockSignals(true);   m_segDl->setCurrentIndex(idx);   m_segDl->blockSignals(false); }
        if (m_segUl)   { m_segUl->blockSignals(true);   m_segUl->setCurrentIndex(idx);   m_segUl->blockSignals(false); }
        if (m_segSync) { m_segSync->blockSignals(true); m_segSync->setCurrentIndex(idx); m_segSync->blockSignals(false); }
        loadCurrentPage();
    };
    connect(m_segDl, &oclero::qlementine::SegmentedControl::currentIndexChanged, this, [this, onSegmentChanged]() { onSegmentChanged(m_segDl->currentIndex()); });
    connect(m_segUl, &oclero::qlementine::SegmentedControl::currentIndexChanged, this, [this, onSegmentChanged]() { onSegmentChanged(m_segUl->currentIndex()); });
    connect(m_segSync, &oclero::qlementine::SegmentedControl::currentIndexChanged, this, [this, onSegmentChanged]() { onSegmentChanged(m_segSync->currentIndex()); });

    m_dockWidget = new KDDockWidgets::QtWidgets::DockWidget( "FilesFeed:Dock-" + w->GetProfile()->GetProject(), KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    m_dockWidget->setTitle("Files");
    m_dockWidget->setIcon(QIcon(":/icons/downloads"), KDDockWidgets::IconPlace::TabBar);
    m_dockWidget->setWidget(this);

    pageHelperDl = new PagedTableHelper(w->GetProfile(), "/download/list", this);
    pageHelperUl = new PagedTableHelper(w->GetProfile(), "/upload/list", this);

    connect(pageHelperDl, &PagedTableHelper::pageReady, this, &FilesFeedWidget::onDlPageReady);
    connect(pageHelperDl, &PagedTableHelper::errorOccurred, this, &FilesFeedWidget::onDlPageError);
    connect(pageHelperUl, &PagedTableHelper::pageReady, this, &FilesFeedWidget::onUlPageReady);
    connect(pageHelperUl, &PagedTableHelper::errorOccurred, this, &FilesFeedWidget::onUlPageError);

    auto* dlPbar = dlFeed->paginationBar();
    if (dlPbar) {
        connect(dlPbar, &PaginationBar::prevClicked, this, [this]() {
            m_dlOffset = qMax(0, m_dlOffset - dlFeed->paginationBar()->pageSize());
            loadCurrentPage();
        });
        connect(dlPbar, &PaginationBar::nextClicked, this, [this]() {
            m_dlOffset += dlFeed->paginationBar()->pageSize();
            loadCurrentPage();
        });
        connect(dlPbar, &PaginationBar::pageSizeChanged, this, [this](int) {
            m_dlOffset = 0;
            loadCurrentPage();
        });
        pageHelperDl->setPageSize(dlPbar->pageSize());
    }
    auto* dlSearch = dlFeed->searchInput();
    if (dlSearch) {
        connect(dlSearch, &QLineEdit::returnPressed, this, [this]() { m_dlOffset = 0; loadCurrentPage(); });
    }
    connect(dlFeed->filterCombo(), &QComboBox::currentTextChanged, this, [this](const QString& text) {
        bool filterAll = (text == "All computers");
        for (int i = 0; i < dlModel->size(); ++i) {
            if (filterAll) {
                dlFeed->treeView()->setRowHidden(i, QModelIndex(), false);
                continue;
            }
            QString comp = dlModel->rowAt(i).blockData[FFB_Info].toMap()["main"].toString();
            int at = comp.indexOf(" @ ");
            if (at >= 0)
                comp = comp.mid(at + 3);
            dlFeed->treeView()->setRowHidden(i, QModelIndex(), comp != text);
        }
    });

    connect(dlFeed->treeView(), &QTreeView::customContextMenuRequested, this, &FilesFeedWidget::handleDownloadsMenu);

    auto* ulPbar = ulFeed->paginationBar();
    if (ulPbar) {
        connect(ulPbar, &PaginationBar::prevClicked, this, [this]() {
            m_ulOffset = qMax(0, m_ulOffset - ulFeed->paginationBar()->pageSize());
            loadCurrentPage();
        });
        connect(ulPbar, &PaginationBar::nextClicked, this, [this]() {
            m_ulOffset += ulFeed->paginationBar()->pageSize();
            loadCurrentPage();
        });
        connect(ulPbar, &PaginationBar::pageSizeChanged, this, [this](int) {
            m_ulOffset = 0;
            loadCurrentPage();
        });
        pageHelperUl->setPageSize(ulPbar->pageSize());
    }
    auto* ulSearch = ulFeed->searchInput();
    if (ulSearch) {
        connect(ulSearch, &QLineEdit::returnPressed, this, [this]() { m_ulOffset = 0; loadCurrentPage(); });
    }
    connect(ulFeed->filterCombo(), &QComboBox::currentTextChanged, this, [this](const QString& text) {
        bool filterAll = (text == "All computers");
        for (int i = 0; i < ulModel->size(); ++i) {
            if (filterAll) {
                ulFeed->treeView()->setRowHidden(i, QModelIndex(), false);
                continue;
            }
            QString comp = ulModel->rowAt(i).blockData[FFB_Info].toMap()["main"].toString();
            int at = comp.indexOf(" @ ");
            if (at >= 0)
                comp = comp.mid(at + 3);
            ulFeed->treeView()->setRowHidden(i, QModelIndex(), comp != text);
        }
    });

    connect(ulFeed->treeView(), &QTreeView::customContextMenuRequested, this, &FilesFeedWidget::handleUploadsMenu);
    connect(syncFeed->treeView(), &QTreeView::customContextMenuRequested, this, &FilesFeedWidget::handleSyncMenu);

    auto* syncSearch = syncFeed->searchInput();
    if (syncSearch) {
        connect(syncSearch, &QLineEdit::textChanged, this, [this](const QString& text) {
            for (int i = 0; i < syncModel->size(); ++i) {
                bool match = text.isEmpty() ||
                    syncModel->rowAt(i).blockData[FFB_Main].toMap()["main"].toString().contains(text, Qt::CaseInsensitive) ||
                    syncModel->rowAt(i).blockData[FFB_Main].toMap()["second"].toString().contains(text, Qt::CaseInsensitive) ||
                    syncModel->rowAt(i).blockData[FFB_Id].toMap()["id"].toString().contains(text, Qt::CaseInsensitive);
                syncFeed->treeView()->setRowHidden(i, QModelIndex(), !match);
            }
        });
    }

    loadCurrentPage();
}

FilesFeedWidget::~FilesFeedWidget() = default;

KDDockWidgets::QtWidgets::DockWidget* FilesFeedWidget::dock() { return m_dockWidget; }

void FilesFeedWidget::setSegment(int index)
{
    if (index < 0 || index > 2)
        return;
    if (m_currentSegment == index && m_stack && m_stack->currentIndex() == index)
        return;
    m_currentSegment = index;
    if (m_stack)
        m_stack->setCurrentIndex(index);
    if (m_segDl)   { m_segDl->blockSignals(true);   m_segDl->setCurrentIndex(index);   m_segDl->blockSignals(false); }
    if (m_segUl)   { m_segUl->blockSignals(true);   m_segUl->setCurrentIndex(index);   m_segUl->blockSignals(false); }
    if (m_segSync) { m_segSync->blockSignals(true); m_segSync->setCurrentIndex(index); m_segSync->blockSignals(false); }
    loadCurrentPage();
}

ListFeedWidget* FilesFeedWidget::activeFeed() const
{
    if (m_currentSegment == 0)
        return dlFeed;
    if (m_currentSegment == 1)
        return ulFeed;
    return syncFeed;
}

FeedListModel* FilesFeedWidget::activeModel() const
{
    if (m_currentSegment == 0)
        return dlModel;
    if (m_currentSegment == 1)
        return ulModel;
    return syncModel;
}

void FilesFeedWidget::loadCurrentPage()
{
    if (m_currentSegment == 0) {
        auto* si = dlFeed->searchInput();
        pageHelperDl->setParam("q", si ? si->text() : QString());
        pageHelperDl->setParam("sort", m_dlSortCol);
        pageHelperDl->setParam("order", m_dlSortOrder);
        pageHelperDl->setPageSize(dlFeed->paginationBar() ? dlFeed->paginationBar()->pageSize() : 100);
        pageHelperDl->loadPage(m_dlOffset);
    } else if (m_currentSegment == 1) {
        auto* si = ulFeed->searchInput();
        pageHelperUl->setParam("q", si ? si->text() : QString());
        pageHelperUl->setParam("sort", m_ulSortCol);
        pageHelperUl->setParam("order", m_ulSortOrder);
        pageHelperUl->setPageSize(ulFeed->paginationBar() ? ulFeed->paginationBar()->pageSize() : 100);
        pageHelperUl->loadPage(m_ulOffset);
    }
}

void FilesFeedWidget::SetUpdatesEnabled(bool) {}

void FilesFeedWidget::Clear()
{
    m_dlCache.clear();
    m_ulCache.clear();
    m_syncCache.clear();
    dlModel->clear();
    ulModel->clear();
    syncModel->clear();
    m_dlOffset = 0;
    m_ulOffset = 0;
}

void FilesFeedWidget::setCompactMode(bool compact)
{
    if (dlFeed) dlFeed->setCompactMode(compact);
    if (ulFeed) ulFeed->setCompactMode(compact);
    if (syncFeed) syncFeed->setCompactMode(compact);
}

void FilesFeedWidget::UpdateColumnsVisible()
{
    auto rebuildModel = [](FeedListModel* model, const QHash<qint64, TransferData>& cache, bool isDownload) {
        if (!model) return;
        for (int i = 0; i < model->size(); ++i) {
            const FeedRow& r = model->rowAt(i);
            auto it = cache.constFind(r.entityId);
            if (it == cache.constEnd())
                continue;
            model->updateRow(i, transferToFeedRow(it.value(), isDownload));
        }
    };
    rebuildModel(dlModel, m_dlCache, true);
    rebuildModel(ulModel, m_ulCache, false);

    if (syncModel) {
        for (int i = 0; i < syncModel->size(); ++i) {
            const FeedRow& r = syncModel->rowAt(i);
            QString syncId = r.blockData[FFB_Id].toMap().value("syncId").toString();
            if (syncId.isEmpty())
                syncId = r.blockData[FFB_Id].toMap().value("id").toString();
            auto it = m_syncCache.constFind(syncId);
            if (it == m_syncCache.constEnd())
                continue;
            syncModel->updateRow(i, syncToFeedRow(it.value()));
        }
    }

    auto updateDelegate = [](ListFeedWidget* feed, FeedListModel* model) {
        if (!feed || !model) return;
        auto* delegate = qobject_cast<ListFeedDelegate*>(feed->treeView()->itemDelegate());
        if (delegate)
            delegate->updateMaxWidths(model);
    };
    updateDelegate(dlFeed, dlModel);
    updateDelegate(ulFeed, ulModel);
    updateDelegate(syncFeed, syncModel);
}

void FilesFeedWidget::AddTransferItem(const TransferData& transfer)
{
    bool isDownload = (transfer.TransferType == TRANSFER_DOWNLOAD);
    FeedListModel* model = isDownload ? dlModel : ulModel;
    FeedRow row = transferToFeedRow(transfer, isDownload);

    if (isDownload) {
        m_dlCache[transfer.FileId] = transfer;
        QWriteLocker locker(&m_adaptixWidget->DownloadsLock);
        m_adaptixWidget->Downloads[transfer.FileId] = transfer;
    } else {
        m_ulCache[transfer.FileId] = transfer;
        QWriteLocker locker(&m_adaptixWidget->UploadsLock);
        m_adaptixWidget->Uploads[transfer.FileId] = transfer;
    }

    for (int i = 0; i < model->size(); ++i) {
        if (model->rowAt(i).entityId == transfer.FileId) {
            model->updateRow(i, row);
            return;
        }
    }
    model->insertRow(0, row);
}

void FilesFeedWidget::EditTransferItem(int transferType, qint64 fileId, qint64 progress, int state)
{
    FeedListModel* model = (transferType == TRANSFER_DOWNLOAD) ? dlModel : ulModel;
    bool isDownload = (transferType == TRANSFER_DOWNLOAD);
    auto& cache = isDownload ? m_dlCache : m_ulCache;

    {
        QWriteLocker locker(isDownload ? &m_adaptixWidget->DownloadsLock : &m_adaptixWidget->UploadsLock);
        auto& srcMap = isDownload ? m_adaptixWidget->Downloads : m_adaptixWidget->Uploads;
        auto it = srcMap.find(fileId);
        if (it != srcMap.end()) {
            it->Progress = progress;
            it->State = state;
            cache[fileId] = it.value();
        }
    }

    if (cache.contains(fileId)) {
        TransferData t = cache[fileId];
        t.Progress = progress;
        t.State = state;
        cache[fileId] = t;
        for (int i = 0; i < model->size(); ++i) {
            if (model->rowAt(i).entityId == fileId) {
                model->updateRow(i, transferToFeedRow(t, isDownload));
                return;
            }
        }
    }
}

void FilesFeedWidget::RemoveTransferItem(int transferType, const QList<qint64>& filesId)
{
    FeedListModel* model = (transferType == TRANSFER_DOWNLOAD) ? dlModel : ulModel;
    auto& cache = (transferType == TRANSFER_DOWNLOAD) ? m_dlCache : m_ulCache;
    const bool isDownload = (transferType == TRANSFER_DOWNLOAD);
    for (qint64 fileId : filesId) {
        cache.remove(fileId);
        {
            QWriteLocker locker(isDownload ? &m_adaptixWidget->DownloadsLock : &m_adaptixWidget->UploadsLock);
            auto& srcMap = isDownload ? m_adaptixWidget->Downloads : m_adaptixWidget->Uploads;
            srcMap.remove(fileId);
        }
        for (int i = 0; i < model->size(); ++i) {
            if (model->rowAt(i).entityId == fileId) {
                model->removeRow(i);
                break;
            }
        }
    }
}

void FilesFeedWidget::SetTransferTag(int transferType, const QList<qint64>& filesId, const QString& tag)
{
    FeedListModel* model = (transferType == TRANSFER_DOWNLOAD) ? dlModel : ulModel;
    bool isDownload = (transferType == TRANSFER_DOWNLOAD);
    auto& cache = isDownload ? m_dlCache : m_ulCache;
    for (qint64 fileId : filesId) {
        if (cache.contains(fileId)) {
            TransferData t = cache[fileId];
            t.Tag = tag;
            cache[fileId] = t;
            for (int i = 0; i < model->size(); ++i) {
                if (model->rowAt(i).entityId == fileId) {
                    model->updateRow(i, transferToFeedRow(t, isDownload));
                    break;
                }
            }
        }
    }
}

void FilesFeedWidget::AddSyncEntry(const SyncEntryData& entry)
{
    m_syncCache[entry.id] = entry;
    FeedRow row = syncToFeedRow(entry);
    for (int i = 0; i < syncModel->size(); ++i) {
        QString rowId = syncModel->rowAt(i).blockData[FFB_Id].toMap().value("syncId").toString();
        if (rowId.isEmpty())
            rowId = syncModel->rowAt(i).blockData[FFB_Id].toMap()["id"].toString();
        if (rowId == entry.id || rowId == entry.id.left(8)) {
            syncModel->updateRow(i, row);
            return;
        }
    }
    syncModel->insertRow(0, row);
}

void FilesFeedWidget::UpdateSyncEntry(const QString& id, qint64 progress, qint64 totalSize, double speed)
{
    Q_UNUSED(speed);
    auto it = m_syncCache.find(id);
    if (it == m_syncCache.end())
        return;
    it->progress = progress;
    it->totalSize = totalSize;
    it->state = TRANSFER_STATE_RUNNING;
    FeedRow row = syncToFeedRow(it.value());
    for (int i = 0; i < syncModel->size(); ++i) {
        QString rowId = syncModel->rowAt(i).blockData[FFB_Id].toMap().value("syncId").toString();
        if (rowId.isEmpty())
            rowId = syncModel->rowAt(i).blockData[FFB_Id].toMap()["id"].toString();
        if (rowId == id || rowId == id.left(8)) {
            syncModel->updateRow(i, row);
            return;
        }
    }
}

void FilesFeedWidget::FinishSyncEntry(const QString& id, int state)
{
    auto it = m_syncCache.find(id);
    if (it == m_syncCache.end())
        return;
    it->state = state;
    if (state == TRANSFER_STATE_FINISHED)
        it->progress = it->totalSize;
    FeedRow row = syncToFeedRow(it.value());
    for (int i = 0; i < syncModel->size(); ++i) {
        QString rowId = syncModel->rowAt(i).blockData[FFB_Id].toMap().value("syncId").toString();
        if (rowId.isEmpty())
            rowId = syncModel->rowAt(i).blockData[FFB_Id].toMap()["id"].toString();
        if (rowId == id || rowId == id.left(8)) {
            syncModel->updateRow(i, row);
            return;
        }
    }
}

void FilesFeedWidget::onDlPageReady(const QJsonObject& response)
{
    QJsonArray items = response["items"].toArray();
    QList<TransferData> page;
    page.reserve(items.size());
    for (const QJsonValue& v : items) {
        QJsonObject obj = v.toObject();
        TransferData t;
        t.FileId       = parseI64(obj, "t_file_id");
        t.AgentId      = parseI64(obj, "t_agent_id");
        t.AgentName    = obj["t_agent_name"].toString();
        t.User         = obj["t_user"].toString();
        t.Computer     = obj["t_computer"].toString();
        t.Filename     = obj["t_remote_path"].toString();
        t.TotalSize    = parseI64(obj, "t_total_size");
        t.Progress     = parseI64(obj, "t_progress");
        t.DateTimestamp = parseI64(obj, "t_date");
        t.State        = obj["t_state"].toInt();
        t.Tag          = obj["t_tag"].toString();
        t.Kind         = obj["t_kind"].toInt();
        page.append(t);
    }

    m_dlOffset = response["offset"].toInt();
    int total = response["total"].toInt();

    m_dlCache.clear();
    dlModel->clear();
    for (const auto& t : page) {
        m_dlCache[t.FileId] = t;
        dlModel->addRow(transferToFeedRow(t, true));
    }
    if (dlFeed->filterCombo()) {
        QString current = dlFeed->filterCombo()->currentText();
        dlFeed->filterCombo()->blockSignals(true);
        dlFeed->filterCombo()->clear();
        dlFeed->filterCombo()->addItem("All computers");
        QSet<QString> computers;
        for (int i = 0; i < dlModel->size(); ++i) {
            QString comp = dlModel->rowAt(i).blockData[FFB_Info].toMap().value("computer").toString();
            if (comp.isEmpty()) {
                comp = dlModel->rowAt(i).blockData[FFB_Info].toMap()["main"].toString();
                int at = comp.indexOf(" @ ");
                if (at >= 0)
                    comp = comp.mid(at + 3);
            }
            if (!comp.isEmpty())
                computers.insert(comp);
        }
        for (const auto& c : computers) {
            dlFeed->filterCombo()->addItem(c);
        }
        int idx = dlFeed->filterCombo()->findText(current);
        if (idx >= 0)
            dlFeed->filterCombo()->setCurrentIndex(idx);
        dlFeed->filterCombo()->blockSignals(false);
    }

    auto* pbar = dlFeed->paginationBar();
    if (pbar) {
        int pageSize = pbar->pageSize();
        int shown = page.size();
        int from = total == 0 ? 0 : m_dlOffset + 1;
        int to = m_dlOffset + shown;
        pbar->setInfo(from, to, total);
        pbar->setPrevEnabled(m_dlOffset > 0);
        pbar->setNextEnabled(m_dlOffset + pageSize < total);
    }
}

void FilesFeedWidget::onDlPageError(const QString&) { dlModel->clear(); }

void FilesFeedWidget::onUlPageReady(const QJsonObject& response)
{
    QJsonArray items = response["items"].toArray();
    QList<TransferData> page;
    page.reserve(items.size());
    for (const QJsonValue& v : items) {
        QJsonObject obj = v.toObject();
        TransferData t;
        t.FileId       = parseI64(obj, "t_file_id");
        t.AgentId      = parseI64(obj, "t_agent_id");
        t.AgentName    = obj["t_agent_name"].toString();
        t.User         = obj["t_user"].toString();
        t.Computer     = obj["t_computer"].toString();
        t.Filename     = obj["t_remote_path"].toString();
        t.TotalSize    = parseI64(obj, "t_total_size");
        t.Progress     = parseI64(obj, "t_progress");
        t.DateTimestamp = parseI64(obj, "t_date");
        t.State        = obj["t_state"].toInt();
        t.Tag          = obj["t_tag"].toString();
        t.Kind         = obj["t_kind"].toInt();
        page.append(t);
    }

    m_ulOffset = response["offset"].toInt();
    int total = response["total"].toInt();

    m_ulCache.clear();
    ulModel->clear();
    for (const auto& t : page) {
        m_ulCache[t.FileId] = t;
        ulModel->addRow(transferToFeedRow(t, false));
    }
    if (ulFeed->filterCombo()) {
        QString current = ulFeed->filterCombo()->currentText();
        ulFeed->filterCombo()->blockSignals(true);
        ulFeed->filterCombo()->clear();
        ulFeed->filterCombo()->addItem("All computers");
        QSet<QString> computers;
        for (int i = 0; i < ulModel->size(); ++i) {
            QString comp = ulModel->rowAt(i).blockData[FFB_Info].toMap().value("computer").toString();
            if (comp.isEmpty()) {
                comp = ulModel->rowAt(i).blockData[FFB_Info].toMap()["main"].toString();
                int at = comp.indexOf(" @ ");
                if (at >= 0)
                    comp = comp.mid(at + 3);
            }
            if (!comp.isEmpty()) computers.insert(comp);
        }
        for (const auto& c : computers) {
            ulFeed->filterCombo()->addItem(c);
        }
        int idx = ulFeed->filterCombo()->findText(current);
        if (idx >= 0)
            ulFeed->filterCombo()->setCurrentIndex(idx);
        ulFeed->filterCombo()->blockSignals(false);
    }

    auto* pbar = ulFeed->paginationBar();
    if (pbar) {
        int pageSize = pbar->pageSize();
        int shown = page.size();
        int from = total == 0 ? 0 : m_ulOffset + 1;
        int to = m_ulOffset + shown;
        pbar->setInfo(from, to, total);
        pbar->setPrevEnabled(m_ulOffset > 0);
        pbar->setNextEnabled(m_ulOffset + pageSize < total);
    }
}

void FilesFeedWidget::onUlPageError(const QString&) { ulModel->clear(); }

void FilesFeedWidget::handleSyncMenu(const QPoint& pos)
{
    if (!syncFeed->prepareContextMenuSelection(pos).isValid())
        return;

    auto mapToSourceRow = [this](const QModelIndex& idx) -> int {
        QModelIndex src = idx;
        QAbstractItemModel* m = const_cast<QAbstractItemModel*>(idx.model());
        while (m && m != syncModel) {
            auto* sp = qobject_cast<QSortFilterProxyModel*>(m);
            if (sp) {
                src = sp->mapToSource(src);
                m = sp->sourceModel();
                continue;
            }
            auto* gp = qobject_cast<GroupingProxyModel*>(m);
            if (gp) {
                src = gp->mapToSource(src);
                m = gp->sourceModel();
                continue;
            }
            break;
        }
        return (src.isValid() && src.row() >= 0 && src.row() < syncModel->size()) ? src.row() : -1;
    };

    QList<int> sourceRows;
    for (const QModelIndex& idx : syncFeed->treeView()->selectionModel()->selectedRows()) {
        int r = mapToSourceRow(idx);
        if (r >= 0 && !sourceRows.contains(r))
            sourceRows.append(r);
    }
    if (sourceRows.isEmpty())
        return;

    oclero::qlementine::Menu ctxMenu;

    if (sourceRows.size() == 1) {
        int row = sourceRows.first();
        QString localPath = syncModel->rowAt(row).blockData[FFB_Main].toMap()["second"].toString();

        if (!localPath.isEmpty()) {
            ctxMenu.addAction(QIcon(":/icons/notes"), "Copy file path", this, [localPath]() {
                QGuiApplication::clipboard()->setText(localPath);
                MessageSuccess("Path: " + localPath);
            });
            ctxMenu.addAction(QIcon(":/icons/open_folder"), "Open directory", this, [localPath]() {
                QFileInfo fi(localPath);
                QString dir = fi.absolutePath();
                if (QDir(dir).exists()) {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
                } else {
                    MessageError("Directory not found: " + dir);
                }
            });
            ctxMenu.addSeparator();
        }
    }

    ctxMenu.addAction(QIcon(":/icons/delete"), sourceRows.size() > 1 ? QString("Delete %1 entries").arg(sourceRows.size()) : "Delete entry", this, [this, sourceRows]() {
        QList<int> sorted = sourceRows;
        std::sort(sorted.begin(), sorted.end(), [](int a, int b) { return a > b; });
        for (int r : sorted) {
            if (r >= 0 && r < syncModel->size())
                syncModel->removeRow(r);
        }
    });
    ctxMenu.exec(syncFeed->treeView()->viewport()->mapToGlobal(pos));
}

void FilesFeedWidget::UpdateFilterComboBoxes() const {}

qint64 FilesFeedWidget::getSelectedFileId() const
{
    auto* feed = activeFeed();
    QModelIndex idx = feed->treeView()->currentIndex();
    if (!idx.isValid())
        return 0;

    return feed->proxyModel()->data(idx, Qt::UserRole).toLongLong();
}

QList<const TransferData*> FilesFeedWidget::getSelectedDownloads() const
{
    QList<const TransferData*> result;
    QModelIndexList selected = dlFeed->treeView()->selectionModel()->selectedRows();
    for (const QModelIndex& idx : selected) {
        qint64 fileId = dlFeed->proxyModel()->data(idx, Qt::UserRole).toLongLong();
        if (fileId) {
            QReadLocker locker(&m_adaptixWidget->DownloadsLock);
            auto it = m_adaptixWidget->Downloads.constFind(fileId);
            if (it != m_adaptixWidget->Downloads.constEnd())
                result.append(&it.value());
        }
    }
    return result;
}

QList<const TransferData*> FilesFeedWidget::getSelectedUploads() const
{
    QList<const TransferData*> result;
    QModelIndexList selected = ulFeed->treeView()->selectionModel()->selectedRows();
    for (const QModelIndex& idx : selected) {
        qint64 fileId = ulFeed->proxyModel()->data(idx, Qt::UserRole).toLongLong();
        if (fileId) {
            QReadLocker locker(&m_adaptixWidget->UploadsLock);
            auto it = m_adaptixWidget->Uploads.constFind(fileId);
            if (it != m_adaptixWidget->Uploads.constEnd())
                result.append(&it.value());
        }
    }
    return result;
}

bool FilesFeedWidget::modelContainsTransfer(int transferType, qint64 fileId) const
{
    const FeedListModel* model = (transferType == TRANSFER_DOWNLOAD) ? dlModel : ulModel;
    for (int i = 0; i < model->size(); ++i) {
        if (model->rowAt(i).entityId == fileId)
            return true;
    }
    return false;
}

void FilesFeedWidget::handleFeedMenu(const QPoint& pos)
{
    if (m_currentSegment == 0) {
        handleDownloadsMenu(pos);
        return;
    }
    if (m_currentSegment == 1) {
        handleUploadsMenu(pos);
        return;
    }
}

void FilesFeedWidget::handleDownloadsMenu(const QPoint& pos)
{
    if (!dlFeed->prepareContextMenuSelection(pos).isValid())
        return;

    QModelIndexList selected = dlFeed->treeView()->selectionModel()->selectedRows();
    QList<qint64> selectedIds;
    QList<qint64> finishedIds;
    for (const QModelIndex& idx : selected) {
        qint64 fid = dlFeed->proxyModel()->data(idx, Qt::UserRole).toLongLong();
        if (!fid)
            continue;

        selectedIds.append(fid);
        for (int i = 0; i < dlModel->size(); ++i) {
            if (dlModel->rowAt(i).entityId == fid) {
                QVariantMap right = dlModel->rowAt(i).blockData[FFB_Right].toMap();
                if (right["status"].toString() == "Finished")
                    finishedIds.append(fid);
                break;
            }
        }
    }
    if (selectedIds.isEmpty())
        return;

    bool allFinished = !finishedIds.isEmpty() && (finishedIds.size() == selectedIds.size());

    oclero::qlementine::Menu ctxMenu;

    if (selectedIds.size() > 1 && allFinished) {
        ctxMenu.addAction(QString("Sync %1 files to client").arg(finishedIds.size()), this, &FilesFeedWidget::actionSyncMultiple);
        auto* syncMenu = new oclero::qlementine::Menu("Sync as ...", &ctxMenu);
        syncMenu->addAction("Curl command");
        syncMenu->addAction("Wget command");
        QAction* syncAction = ctxMenu.addMenu(syncMenu);
        syncAction->setEnabled(false);
        ctxMenu.addSeparator();

        QVector<DataMenuDownload> files;
        for (qint64 fid : finishedIds) {
            for (int i = 0; i < dlModel->size(); ++i) {
                if (dlModel->rowAt(i).entityId == fid) {
                    DataMenuDownload data = {};
                    data.fileId = fid;
                    data.path = dlModel->rowAt(i).blockData[FFB_Main].toMap()["second"].toString();
                    data.state = "finished";
                    QReadLocker locker(&m_adaptixWidget->DownloadsLock);
                    auto it = m_adaptixWidget->Downloads.constFind(fid);
                    if (it != m_adaptixWidget->Downloads.constEnd())
                        data.agentId = it->AgentId;
                    files.append(data);
                    break;
                }
            }
        }
        int menuCount = m_adaptixWidget->ScriptManager->AddMenuDownload(&ctxMenu, "DownloadFinished", files, false);
        if (menuCount > 0)
            ctxMenu.addSeparator();

    } else if (selectedIds.size() == 1 && allFinished) {
        ctxMenu.addAction("Sync file to client", this, &FilesFeedWidget::actionSync);
        auto* syncMenu = new oclero::qlementine::Menu("Sync as ...", &ctxMenu);
        syncMenu->addAction("Curl command", this, &FilesFeedWidget::actionSyncCurl);
        syncMenu->addAction("Wget command", this, &FilesFeedWidget::actionSyncWget);
        ctxMenu.addMenu(syncMenu);
        ctxMenu.addSeparator();

        qint64 fid = selectedIds.first();
        DataMenuDownload data = {};
        data.fileId = fid;
        for (int i = 0; i < dlModel->size(); ++i) {
            if (dlModel->rowAt(i).entityId == fid) {
                data.path = dlModel->rowAt(i).blockData[FFB_Main].toMap()["second"].toString();
                break;
            }
        }
        data.state = "finished";
        QReadLocker locker(&m_adaptixWidget->DownloadsLock);
        auto it = m_adaptixWidget->Downloads.constFind(fid);
        if (it != m_adaptixWidget->Downloads.constEnd())
            data.agentId = it->AgentId;
        QVector<DataMenuDownload> files = {data};
        int menuCount = m_adaptixWidget->ScriptManager->AddMenuDownload(&ctxMenu, "DownloadFinished", files, false);
        if (menuCount > 0)
            ctxMenu.addSeparator();
    } else if (selectedIds.size() == 1) {
        qint64 fid = selectedIds.first();
        DataMenuDownload data = {};
        data.fileId = fid;
        for (int i = 0; i < dlModel->size(); ++i) {
            if (dlModel->rowAt(i).entityId == fid) {
                data.path = dlModel->rowAt(i).blockData[FFB_Main].toMap()["second"].toString();
                break;
            }
        }
        data.state = "running";
        QReadLocker locker(&m_adaptixWidget->DownloadsLock);
        auto it = m_adaptixWidget->Downloads.constFind(fid);
        if (it != m_adaptixWidget->Downloads.constEnd()) {
            data.agentId = it->AgentId;
            data.state = (it->State == TRANSFER_STATE_RUNNING) ? "running" : "stopped";
        }
        QVector<DataMenuDownload> files = {data};
        int menuCount = m_adaptixWidget->ScriptManager->AddMenuDownload(&ctxMenu, "DownloadRunning", files, true);
        if (menuCount > 0)
            ctxMenu.addSeparator();
    }

    ctxMenu.addAction(QIcon(":/icons/tag"), "Set tag", this, &FilesFeedWidget::actionSetTag);
    ctxMenu.addAction(QIcon(":/icons/delete"), selectedIds.size() > 1 ? QString("Delete %1 files").arg(selectedIds.size()) : "Delete file", this, &FilesFeedWidget::actionDelete);
    ctxMenu.exec(dlFeed->treeView()->viewport()->mapToGlobal(pos));
}

void FilesFeedWidget::handleUploadsMenu(const QPoint& pos)
{
    if (!ulFeed->prepareContextMenuSelection(pos).isValid())
        return;

    QModelIndexList selected = ulFeed->treeView()->selectionModel()->selectedRows();
    QList<qint64> ids;
    for (const QModelIndex& idx : selected) {
        qint64 fid = ulFeed->proxyModel()->data(idx, Qt::UserRole).toLongLong();
        if (fid) ids.append(fid);
    }
    if (ids.isEmpty())
        return;

    oclero::qlementine::Menu ctxMenu;
    ctxMenu.addAction(QIcon(":/icons/delete"), ids.size() > 1 ? QString("Delete %1 files").arg(ids.size()) : "Delete file", this, &FilesFeedWidget::actionDeleteUploads);
    ctxMenu.exec(ulFeed->treeView()->viewport()->mapToGlobal(pos));
}

void FilesFeedWidget::actionSync()
{
    QModelIndex idx = dlFeed->treeView()->currentIndex();
    if (!idx.isValid())
        return;

    qint64 fileId = dlFeed->proxyModel()->data(idx, Qt::UserRole).toLongLong();
    if (!fileId)
        return;

    QString filePath;
    for (int i = 0; i < dlModel->size(); ++i) {
        if (dlModel->rowAt(i).entityId == fileId) {
            QVariantMap right = dlModel->rowAt(i).blockData[FFB_Right].toMap();
            if (right["status"].toString() != "Finished")
                return;

            filePath = dlModel->rowAt(i).blockData[FFB_Main].toMap()["second"].toString();
            break;
        }
    }
    if (filePath.isEmpty())
        return;

    QString message;
    bool ok = false;
    QJsonObject otpData;
    otpData["id"] = toJsonI64(fileId);
    bool result = HttpReqGetOTP("download", otpData, *(m_adaptixWidget->GetProfile()), &message, &ok);
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
    if (m_adaptixWidget && m_adaptixWidget->GetProfile())
        baseDir = QDir(m_adaptixWidget->GetProfile()->GetProjectDir()).filePath(fileName);

    NonBlockingDialogs::getSaveFileName(this, "Save File", baseDir, "All Files (*.*)",
        [this, otp](const QString& savedPath) {
            if (savedPath.isEmpty())
                return;

            QString sUrl = m_adaptixWidget->GetProfile()->GetURL() + "/otp/download/sync";
            auto* dialog = new DialogDownloader(sUrl, otp, savedPath, this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->show();
        });
}

void FilesFeedWidget::actionSyncMultiple()
{
    QModelIndexList selected = dlFeed->treeView()->selectionModel()->selectedRows();
    QList<QPair<qint64, QString>> finished;
    for (const QModelIndex& idx : selected) {
        qint64 fid = dlFeed->proxyModel()->data(idx, Qt::UserRole).toLongLong();
        if (!fid)
            continue;

        for (int i = 0; i < dlModel->size(); ++i) {
            if (dlModel->rowAt(i).entityId == fid) {
                QVariantMap right = dlModel->rowAt(i).blockData[FFB_Right].toMap();
                if (right["status"].toString() == "Finished") {
                    QString fp = dlModel->rowAt(i).blockData[FFB_Main].toMap()["second"].toString();
                    finished.append({fid, fp});
                }
                break;
            }
        }
    }
    if (finished.size() < 2)
        return;

    QString defaultDir;
    if (m_adaptixWidget && m_adaptixWidget->GetProfile())
        defaultDir = m_adaptixWidget->GetProfile()->GetProjectDir();

    QString dir = QFileDialog::getExistingDirectory(this, "Select directory", defaultDir);
    if (dir.isEmpty())
        return;

    QString baseUrl = m_adaptixWidget->GetProfile()->GetURL() + "/otp/download/sync";
    for (const auto& [fid, fp] : finished) {
        QString message;
        bool ok = false;
        QJsonObject otpData;
        otpData["id"] = toJsonI64(fid);
        if (!HttpReqGetOTP("download", otpData, *(m_adaptixWidget->GetProfile()), &message, &ok) || !ok)
            continue;

        QString otp = message;
        QString fileName = extractFileName(fp);
        QString savedPath = QDir(dir).filePath(fileName);
        auto* dialog = new DialogDownloader(baseUrl, otp, savedPath, this, true);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
    }
}

void FilesFeedWidget::actionSyncCurl()
{
    QModelIndex idx = dlFeed->treeView()->currentIndex();
    if (!idx.isValid())
        return;

    qint64 fileId = dlFeed->proxyModel()->data(idx, Qt::UserRole).toLongLong();
    if (!fileId)
        return;

    QString filePath;
    for (int i = 0; i < dlModel->size(); ++i) {
        if (dlModel->rowAt(i).entityId == fileId) {
            QVariantMap right = dlModel->rowAt(i).blockData[FFB_Right].toMap();
            if (right["status"].toString() != "Finished") return;
            filePath = dlModel->rowAt(i).blockData[FFB_Main].toMap()["second"].toString();
            break;
        }
    }
    if (filePath.isEmpty())
        return;

    QString message;
    bool ok = false;
    QJsonObject otpData;
    otpData["id"] = toJsonI64(fileId);
    bool result = HttpReqGetOTP("download", otpData, *(m_adaptixWidget->GetProfile()), &message, &ok);
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
    QString sUrl = m_adaptixWidget->GetProfile()->GetURL() + "/otp/download/sync";
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

void FilesFeedWidget::actionSyncWget()
{
    QModelIndex idx = dlFeed->treeView()->currentIndex();
    if (!idx.isValid())
        return;

    qint64 fileId = dlFeed->proxyModel()->data(idx, Qt::UserRole).toLongLong();
    if (!fileId)
        return;

    QString filePath;
    for (int i = 0; i < dlModel->size(); ++i) {
        if (dlModel->rowAt(i).entityId == fileId) {
            QVariantMap right = dlModel->rowAt(i).blockData[FFB_Right].toMap();
            if (right["status"].toString() != "Finished")
                return;
            filePath = dlModel->rowAt(i).blockData[FFB_Main].toMap()["second"].toString();
            break;
        }
    }
    if (filePath.isEmpty())
        return;

    QString message;
    bool ok = false;
    QJsonObject otpData;
    otpData["id"] = toJsonI64(fileId);
    bool result = HttpReqGetOTP("download", otpData, *(m_adaptixWidget->GetProfile()), &message, &ok);
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
    QString sUrl = m_adaptixWidget->GetProfile()->GetURL() + "/otp/download/sync";
    QString command = QString("wget --no-check-certificate '%1?otp=%2' -o %3").arg(sUrl).arg(otp).arg(fileName);

    QInputDialog inputDialog;
    inputDialog.setWindowTitle("Sync file as wget");
    inputDialog.setLabelText("Wget command:");
    inputDialog.setTextEchoMode(QLineEdit::Normal);
    inputDialog.setTextValue(command);
    inputDialog.setFixedSize(700, 60);
    inputDialog.move(QGuiApplication::primaryScreen()->geometry().center() - inputDialog.geometry().center());
    inputDialog.exec();
}

void FilesFeedWidget::actionDelete()
{
    QModelIndexList selected;
    if (m_currentSegment == 0)
        selected = dlFeed->treeView()->selectionModel()->selectedRows();
    else
        selected = ulFeed->treeView()->selectionModel()->selectedRows();

    QList<qint64> ids;
    for (const QModelIndex& idx : selected) {
        qint64 fid = (m_currentSegment == 0)
            ? dlFeed->proxyModel()->data(idx, Qt::UserRole).toLongLong()
            : ulFeed->proxyModel()->data(idx, Qt::UserRole).toLongLong();
        if (fid)
            ids.append(fid);
    }
    if (ids.isEmpty())
        return;

    if (m_currentSegment == 0) {
        HttpReqDownloadDelete(ids, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    } else {
        HttpReqUploadDelete(ids, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void FilesFeedWidget::actionDeleteUploads()
{
    QModelIndexList selected = ulFeed->treeView()->selectionModel()->selectedRows();
    QList<qint64> ids;
    for (const QModelIndex& idx : selected) {
        qint64 fid = ulFeed->proxyModel()->data(idx, Qt::UserRole).toLongLong();
        if (fid)
            ids.append(fid);
    }
    if (ids.isEmpty())
        return;

    HttpReqUploadDelete(ids, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void FilesFeedWidget::actionSetTag()
{
    QModelIndexList selected = dlFeed->treeView()->selectionModel()->selectedRows();
    QList<qint64> ids;
    QString currentTag;
    for (const QModelIndex& idx : selected) {
        qint64 fid = dlFeed->proxyModel()->data(idx, Qt::UserRole).toLongLong();
        if (fid) {
            ids.append(fid);
            if (currentTag.isEmpty()) {
                QReadLocker locker(&m_adaptixWidget->DownloadsLock);
                auto it = m_adaptixWidget->Downloads.constFind(fid);
                if (it != m_adaptixWidget->Downloads.constEnd())
                    currentTag = it->Tag;
            }
        }
    }
    if (ids.isEmpty())
        return;

    bool ok;
    QString tag = QInputDialog::getText(this, "Set tag", "Tag:", QLineEdit::Normal, currentTag, &ok);
    if (!ok)
        return;

    HttpReqDownloadSetTag(ids, tag, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}
