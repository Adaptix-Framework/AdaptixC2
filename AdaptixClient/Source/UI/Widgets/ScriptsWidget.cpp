#include <UI/Widgets/ScriptsWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <UI/MainUI.h>
#include <Client/AuthProfile.h>
#include <Client/Extender.h>
#include <Utils/CustomElements/ListFeed.h>
#include <Utils/NonBlockingDialogs.h>
#include <Utils/FontManager.h>
#include <MainAdaptix.h>

#include <oclero/qlementine/widgets/Menu.hpp>
#include <oclero/qlementine/widgets/SegmentedControl.hpp>

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QFileInfo>
#include <QSortFilterProxyModel>

REGISTER_DOCK_WIDGET(ScriptsWidget, "Scripts", false)

namespace ScriptsBlock {
    enum {
        Main  = 0,
        Right = 1,
        Count = 2
    };
}

bool ScriptsFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    Q_UNUSED(sourceParent)
    auto* srcModel = qobject_cast<FeedListModel*>(sourceModel());
    if (!srcModel || sourceRow < 0 || sourceRow >= srcModel->size())
        return true;

    if (m_searchText.isEmpty())
        return true;

    const FeedRow& row = srcModel->rowAt(sourceRow);
    const QString lower = m_searchText.toLower();
    for (int i = 0; i < row.size(); ++i) {
        if (row.blockData[i].toString().toLower().contains(lower))
            return true;
        const auto map = row.blockData[i].toMap();
        for (auto it = map.begin(); it != map.end(); ++it) {
            if (it.value().toString().toLower().contains(lower))
                return true;
        }
        const auto list = row.blockData[i].toStringList();
        for (const auto& s : list) {
            if (s.toLower().contains(lower))
                return true;
        }
    }
    return false;
}

static QString rowId(const FeedRow& row)
{
    return row.blockData.value(ScriptsBlock::Main).toMap().value("id").toString();
}

static ListFeedDelegate* createScriptsDelegate(QObject* parent)
{
    auto* d = new ListFeedDelegate(parent);
    d->addBlock(new MainBlock());
    d->addBlock(new StatusBlock());
    return d;
}

static FeedRow localScriptToFeedRow(const ExtensionFile& ext)
{
    FeedRow row;
    row.resize(ScriptsBlock::Count);
    row.entityId = 0;

    QString status;
    QString statusType;
    if (ext.Enabled) {
        status = "Enable";
        statusType = "success";
    } else if (ext.Message.isEmpty()) {
        status = "Disable";
        statusType = "canceled";
    } else {
        status = "Failed";
        statusType = "error";
    }

    const QString mainText    = ext.Name.isEmpty() ? QFileInfo(ext.FilePath).fileName() : ext.Name;
    const QString secondText  = ext.Message.isEmpty() ? ext.FilePath : (ext.FilePath + "  —  " + ext.Message);

    row[ScriptsBlock::Main] = QVariantMap{
        {"id",      ext.FilePath},
        {"main",    mainText},
        {"submain", ext.Description},
        {"second",  secondText}
    };
    row[ScriptsBlock::Right] = QVariantMap{
        {"main",       QString()},
        {"second",     QString()},
        {"status",     status},
        {"statusType", statusType},
        {"dateNum",    0}
    };
    row.isDead = false;
    return row;
}

static FeedRow serverScriptToFeedRow(const ServerScriptInfo& info)
{
    FeedRow row;
    row.resize(ScriptsBlock::Count);
    row.entityId = 0;

    row[ScriptsBlock::Main] = QVariantMap{
        {"id",      info.name},
        {"main",    info.name},
        {"submain", info.description},
        {"second",  QString()}
    };
    row[ScriptsBlock::Right] = QVariantMap{
        {"main",       QString()},
        {"second",     QString()},
        {"status",     info.enabled ? "Enable" : "Disable"},
        {"statusType", info.enabled ? "success" : "canceled"},
        {"dateNum",    0}
    };
    row.isDead = false;
    return row;
}

static int mapToSourceRow(ListFeedWidget* feed, const QModelIndex& idx)
{
    if (!feed || !idx.isValid())
        return -1;

    QModelIndex srcIdx = idx;
    QAbstractItemModel* m = const_cast<QAbstractItemModel*>(idx.model());
    while (m && m != feed->feedModel()) {
        if (auto* sp = qobject_cast<QSortFilterProxyModel*>(m)) {
            srcIdx = sp->mapToSource(srcIdx);
            m = sp->sourceModel();
            continue;
        }
        if (auto* gp = qobject_cast<GroupingProxyModel*>(m)) {
            srcIdx = gp->mapToSource(srcIdx);
            m = gp->sourceModel();
            continue;
        }
        break;
    }
    return srcIdx.row();
}

ScriptsWidget::ScriptsWidget(AdaptixWidget* w) : DockTab("Scripts", w->GetProfile()->GetProject(), ":/icons/code_blocks"), adaptixWidget(w)
{
    setupLocalFeed();
    setupServerFeed();

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_localFeed);
    m_stack->addWidget(m_serverFeed);
    m_stack->setCurrentIndex(0);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_stack, 1);

    auto makeSegment = [this](ListFeedWidget* feed, int idx) {
        auto* seg = new oclero::qlementine::SegmentedControl(feed);
        seg->addItem("Local");
        seg->addItem("Server");
        seg->setCurrentIndex(idx);
        seg->setFixedHeight(FontManager::instance().typography().segmentHeight);
        feed->addToolbarWidgetBefore(seg);
        return seg;
    };

    auto* segLocal  = makeSegment(m_localFeed, 0);
    auto* segServer = makeSegment(m_serverFeed, 1);

    auto onSegmentChanged = [this, segLocal, segServer](int idx) {
        m_currentSegment = idx;
        m_stack->setCurrentIndex(idx);
        segLocal->blockSignals(true);  segLocal->setCurrentIndex(idx);  segLocal->blockSignals(false);
        segServer->blockSignals(true); segServer->setCurrentIndex(idx); segServer->blockSignals(false);
    };
    connect(segLocal,  &oclero::qlementine::SegmentedControl::currentIndexChanged, this, [onSegmentChanged, segLocal]() { onSegmentChanged(segLocal->currentIndex()); });
    connect(segServer, &oclero::qlementine::SegmentedControl::currentIndexChanged, this, [onSegmentChanged, segServer]() { onSegmentChanged(segServer->currentIndex()); });

    refreshLocalScripts();
    refreshServerScripts();

    if (GlobalClient && GlobalClient->extender)
        connect(GlobalClient->extender, &Extender::extensionChanged, this, &ScriptsWidget::refreshLocalScripts);
    connect(adaptixWidget, &AdaptixWidget::serverScriptsChanged, this, &ScriptsWidget::refreshServerScripts);

    this->dockWidget->setWidget(this);
}

ScriptsWidget::~ScriptsWidget() = default;

void ScriptsWidget::setupLocalFeed()
{
    m_localFeed = new ListFeedWidget(adaptixWidget);
    m_localModel = new FeedListModel(this);

    auto* delegate = createScriptsDelegate(this);
    delegate->setFeedModel(m_localModel);

    m_localFilter = new ScriptsFilterProxy(this);
    m_localFilter->setSourceModel(m_localModel);

    m_localFeed->setModel(m_localModel);
    m_localFeed->setDelegate(delegate);
    m_localFeed->setFilterModel(m_localFilter);
    m_localFeed->enableSearch(true);
    m_localFeed->finalizeSearchWidget();
    m_localFeed->enableCompactSwitch(true);
    m_localFeed->setBlockGap(12);
    m_localFeed->rebuildModelChain();

    if (auto* search = m_localFeed->searchInput()) {
        connect(search, &QLineEdit::textChanged, this, [this](const QString& text) {
            m_localFilter->setSearchText(text);
        });
    }

    auto* addBtn = new QPushButton("+ Add Script", m_localFeed);
    connect(addBtn, &QPushButton::clicked, this, &ScriptsWidget::onLocalLoad);
    m_localFeed->addToolbarWidgetAfter(addBtn);

    auto* tv = m_localFeed->treeView();
    tv->setContextMenuPolicy(Qt::CustomContextMenu);
    tv->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(tv, &QTreeView::customContextMenuRequested, this, &ScriptsWidget::onLocalMenu);
}

void ScriptsWidget::setupServerFeed()
{
    m_serverFeed = new ListFeedWidget(adaptixWidget);
    m_serverModel = new FeedListModel(this);

    auto* delegate = createScriptsDelegate(this);
    delegate->setFeedModel(m_serverModel);

    m_serverFilter = new ScriptsFilterProxy(this);
    m_serverFilter->setSourceModel(m_serverModel);

    m_serverFeed->setModel(m_serverModel);
    m_serverFeed->setDelegate(delegate);
    m_serverFeed->setFilterModel(m_serverFilter);
    m_serverFeed->enableSearch(true);
    m_serverFeed->finalizeSearchWidget();
    m_serverFeed->enableCompactSwitch(true);
    m_serverFeed->setBlockGap(12);
    m_serverFeed->rebuildModelChain();

    if (auto* search = m_serverFeed->searchInput()) {
        connect(search, &QLineEdit::textChanged, this, [this](const QString& text) {
            m_serverFilter->setSearchText(text);
        });
    }

    auto* tv = m_serverFeed->treeView();
    tv->setContextMenuPolicy(Qt::CustomContextMenu);
    tv->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(tv, &QTreeView::customContextMenuRequested, this, &ScriptsWidget::onServerMenu);
}

void ScriptsWidget::refreshLocalScripts()
{
    if (!GlobalClient || !GlobalClient->extender)
        return;

    QVector<FeedRow> rows;
    for (const auto& ext : GlobalClient->extender->extenderFiles)
        rows.append(localScriptToFeedRow(ext));

    m_localModel->clear();
    m_localModel->addRows(rows);
}

void ScriptsWidget::refreshServerScripts()
{
    const QList<ServerScriptInfo> scripts = adaptixWidget->GetServerScripts();

    QVector<FeedRow> rows;
    rows.reserve(scripts.size());
    for (const auto& info : scripts)
        rows.append(serverScriptToFeedRow(info));

    m_serverModel->clear();
    m_serverModel->addRows(rows);
}

void ScriptsWidget::onLocalMenu(const QPoint& pos)
{
    oclero::qlementine::Menu menu;

    QString path;
    bool anyEnabled = false;
    bool anyDisabled = false;

    const QModelIndex idx = m_localFeed->treeView()->indexAt(pos);
    const int row = mapToSourceRow(m_localFeed, idx);
    if (row >= 0 && row < m_localModel->size()) {
        path = rowId(m_localModel->rowAt(row));
        if (GlobalClient && GlobalClient->extender) {
            const auto it = GlobalClient->extender->extenderFiles.constFind(path);
            if (it != GlobalClient->extender->extenderFiles.constEnd()) {
                if (it->Enabled)
                    anyEnabled = true;
                else
                    anyDisabled = true;
            }
        }
    }

    const bool hasSelection = !path.isEmpty();

    auto* reloadAction = menu.addAction("Reload", this, [this, path]() { onLocalReload({path}); });
    reloadAction->setEnabled(hasSelection);

    auto* enableAction = menu.addAction("Enable", this, [this, path]() { onLocalEnable({path}); });
    enableAction->setEnabled(hasSelection && anyDisabled);

    auto* disableAction = menu.addAction("Disable", this, [this, path]() { onLocalDisable({path}); });
    disableAction->setEnabled(hasSelection && anyEnabled);

    menu.addSeparator();
    auto* devAction = menu.addAction("Open in DevTools", this, [this, path]() {
        adaptixWidget->OpenInDevTools(path);
    });
    devAction->setEnabled(hasSelection);

    menu.addSeparator();
    auto* removeAction = menu.addAction("Remove", this, [this, path]() { onLocalRemove({path}); });
    removeAction->setEnabled(hasSelection);

    menu.exec(m_localFeed->treeView()->viewport()->mapToGlobal(pos));
}

void ScriptsWidget::onLocalLoad()
{
    QString baseDir;
    if (GlobalClient && GlobalClient->mainUI) {
        if (auto profile = GlobalClient->mainUI->GetCurrentProfile())
            baseDir = profile->GetProjectDir();
    }

    NonBlockingDialogs::getOpenFileName(this, "Load Script", baseDir, "AxScript Files (*.axs)",
        [this](const QString& filePath) {
            if (filePath.isEmpty())
                return;

            GlobalClient->extender->LoadFromFile(filePath, true);
        });
}

void ScriptsWidget::onLocalReload(const QStringList& paths)
{
    if (!GlobalClient || !GlobalClient->extender)
        return;

    for (const auto& path : paths) {
        GlobalClient->extender->RemoveExtension(path);
        GlobalClient->extender->LoadFromFile(path, true);
    }
}

void ScriptsWidget::onLocalEnable(const QStringList& paths)
{
    if (!GlobalClient || !GlobalClient->extender)
        return;

    for (const auto& path : paths)
        GlobalClient->extender->EnableExtension(path);
}

void ScriptsWidget::onLocalDisable(const QStringList& paths)
{
    if (!GlobalClient || !GlobalClient->extender)
        return;

    for (const auto& path : paths)
        GlobalClient->extender->DisableExtension(path);
}

void ScriptsWidget::onLocalRemove(const QStringList& paths)
{
    if (!GlobalClient || !GlobalClient->extender)
        return;

    for (const auto& path : paths)
        GlobalClient->extender->RemoveExtension(path);
}

void ScriptsWidget::onServerMenu(const QPoint& pos)
{
    QString name;
    bool anyEnabled = false;
    bool anyDisabled = false;

    const QModelIndex idx = m_serverFeed->treeView()->indexAt(pos);
    const int row = mapToSourceRow(m_serverFeed, idx);
    if (row >= 0 && row < m_serverModel->size()) {
        name = rowId(m_serverModel->rowAt(row));
        for (const auto& info : adaptixWidget->GetServerScripts()) {
            if (info.name == name) {
                if (info.enabled)
                    anyEnabled = true;
                else
                    anyDisabled = true;
                break;
            }
        }
    }

    const bool hasSelection = !name.isEmpty();

    oclero::qlementine::Menu menu;
    auto* enableAction = menu.addAction("Enable", this, [this, name]() { onServerEnable({name}); });
    enableAction->setEnabled(hasSelection && anyDisabled);

    auto* disableAction = menu.addAction("Disable", this, [this, name]() { onServerDisable({name}); });
    disableAction->setEnabled(hasSelection && anyEnabled);

    menu.exec(m_serverFeed->treeView()->viewport()->mapToGlobal(pos));
}

void ScriptsWidget::onServerEnable(const QStringList& names)
{
    for (const auto& name : names)
        adaptixWidget->EnableServerScript(name);
}

void ScriptsWidget::onServerDisable(const QStringList& names)
{
    for (const auto& name : names)
        adaptixWidget->DisableServerScript(name);
}
