#include <Agent/Agent.h>
#include <functional>
#include <UI/Widgets/SessionsTableWidget.h>
#include <UI/Widgets/BrowserFilesWidget.h>
#include <UI/Widgets/BrowserProcessWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/TerminalContainerWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/TasksFeedWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <UI/Dialogs/DialogTunnel.h>
#include <UI/Dialogs/DialogAgentData.h>
#include <Utils/CustomElements/Delegates.h>
#include <Utils/CustomElements/BoldHeaderView.h>
#include <Utils/Convert.h>
#include <Utils/Logs.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/Requestor.h>
#include <Client/Settings.h>
#include <Client/TunnelEndpoint.h>
#include <Client/AuthProfile.h>
#include <MainAdaptix.h>
#include <Utils/FontManager.h>

#include <oclero/qlementine/widgets/Menu.hpp>
#include <QToolButton>

QVariant AgentsTableModel::data(const QModelIndex &index, const int role) const {
        if (!index.isValid())
            return {};

        qint64 agentId = agentsId.at(index.row());
        Agent*  agent   = adaptixWidget->AgentsMap.value(agentId, nullptr);
        if (!agent)
            return {};

        AgentData d = agent->data;

        if (role == Qt::DisplayRole) {
            switch (index.column()) {
                case SC_Icon:      return {};
                case SC_AgentID:   return QString("#%1").arg(d.Id);
                case SC_AgentType: return d.Name;
                case SC_External:  return d.ExternalIP;
                case SC_Listener:  return d.Listener;
                case SC_Internal:  return d.InternalIP;
                case SC_Domain:    return d.Domain;
                case SC_Computer:  return d.Computer;
                case SC_User:
                {
                    QString username = d.Username;
                    if ( d.Elevated ) username = "* " + username;
                    if ( d.Impersonated != "" ) username += " [" + d.Impersonated + "]";
                    return username;
                }
                case SC_Os:        return d.OsDesc;
                case SC_Process:
                {
                    QString process = d.Process;
                    if ( !d.Arch.isEmpty() )
                        process += QString(" (%2)").arg(d.Arch);
                    return process;
                }
                case SC_Pid:       return d.Pid;
                case SC_Tid:       return d.Tid;
                case SC_Tags:      return d.Tags;
                case SC_Created:   return d.Date;
                case SC_Last:
                {
                    if ( d.Mark.isEmpty() || d.Mark == "No response" || d.Mark == "No worktime" ) {
                        return agent->LastMark;
                    }
                    return UnixTimestampGlobalToStringLocalSmall(d.LastTick);
                }
                case SC_Sleep:
                {
                    if ( d.Mark.isEmpty() ) {
                        if ( !d.Async ) {
                            if ( agent->connType == "internal" )
                                return QString::fromUtf8("\u221E  \u221E");
                            else
                                return QString::fromUtf8("\u27F6\u27F6\u27F6");
                        }
                        return QString("%1 (%2%)").arg( FormatSecToStr(d.Sleep) ).arg(d.Jitter);
                    }
                    return d.Mark;
                }
                default: ;
            }
        }

        if (role == Qt::UserRole) {
            switch (index.column()) {
                case SC_Icon:
                case SC_AgentID: return static_cast<qlonglong>(d.Id);
                case SC_Last:    return d.LastTick;
                case SC_Created: return d.DateTimestamp;
                default:         return data(index, Qt::DisplayRole);
            }
        }

        if (role == Qt::TextAlignmentRole) {
            switch (index.column()) {
                case SC_Icon:
                case SC_AgentType:
                case SC_External:
                case SC_Internal:
                case SC_Listener:
                case SC_Domain:
                case SC_Computer:
                case SC_User:
                case SC_Os:
                case SC_Pid:
                case SC_Tid:
                case SC_Created:
                case SC_Last:
                case SC_Sleep:
                    return Qt::AlignCenter;
                default: ;
            }
        }

        if (role == Qt::DecorationRole) {
            if (index.column() == SC_Icon)
                return agent->iconOs;
        }

        bool dead = (d.Mark == "Terminated" || d.Mark == "Inactive" || d.Mark == "Disconnect" || d.Mark == "No response" || d.Mark == "No worktime");

        if (role == Qt::BackgroundRole) {
            if (dead) {
                bool dk = QPalette().color(QPalette::Base).lightnessF() < 0.5;
                if (agent->bg_color.isValid()) {
                    float h, s, l, a;
                    agent->bg_color.getHslF(&h, &s, &l, &a);
                    l = dk ? qMin(l + static_cast<float>(GlobalClient->settings->data.DeadLightnessShift), 1.0f) : qMax(l - static_cast<float>(GlobalClient->settings->data.DeadLightnessShift), 0.0f);
                    return QColor::fromHslF(h, s, l, a);
                }
                QColor base = QPalette().color(QPalette::Base);
                float h, s, l, a;
                base.getHslF(&h, &s, &l, &a);
                l = dk ? qMin(l + static_cast<float>(GlobalClient->settings->data.DeadLightnessShift), 1.0f) : qMax(l - static_cast<float>(GlobalClient->settings->data.DeadLightnessShift), 0.0f);
                return QColor::fromHslF(h, s, l, a);
            }
            if (agent->bg_color.isValid())
                return agent->bg_color;
            return QVariant();
        }
        if (role == Qt::ForegroundRole) {
            if (agent->fg_color.isValid())
                return agent->fg_color;
            return QVariant();
        }

        if (role == Qt::ToolTipRole) {
            if (index.column() == SC_Sleep) {
                QString WorkAndKill = "";
                if (d.WorkingTime || d.KillDate) {
                    if (d.WorkingTime) {
                        uint startH = ( d.WorkingTime >> 24 ) % 64;
                        uint startM = ( d.WorkingTime >> 16 ) % 64;
                        uint endH   = ( d.WorkingTime >>  8 ) % 64;
                        uint endM   = ( d.WorkingTime >>  0 ) % 64;

                        QChar c = QLatin1Char('0');
                        WorkAndKill = QString("Work time: %1:%2 - %3:%4\n").arg(startH, 2, 10, c).arg(startM, 2, 10, c).arg(endH, 2, 10, c).arg(endM, 2, 10, c);
                    }
                    if (d.KillDate) {
                        QDateTime dateTime = QDateTime::fromSecsSinceEpoch(d.KillDate);
                        WorkAndKill += QString("Kill date: %1").arg(dateTime.toString("dd.MM.yyyy hh:mm:ss"));
                    }
                }
                return WorkAndKill;
            }
        }

        return QVariant();
    }

QVariant AgentsTableModel::headerData(int section, Qt::Orientation o, int role) const {
        if (role != Qt::DisplayRole || o != Qt::Horizontal)
            return {};

        static QStringList headers = {
            QString(), // Icon — empty header
            "ID", "Type", "External", "Listener", "Internal",
            "Domain", "Computer", "User", "OS", "Process",
            "PID", "TID", "Tags", "Created", "Last", "Sleep"
        };

        return headers.value(section);
    }

namespace {
constexpr int kSessionsOsIconSize = 22;
constexpr int kSessionsIconColumnWidth = kSessionsOsIconSize + 6;

void applySessionsIconColumnSize(ColorAwareTreeView* tableView)
{
    if (!tableView)
        return;
    tableView->setIconSize(QSize(kSessionsOsIconSize, kSessionsOsIconSize));
    tableView->header()->setSectionResizeMode(SC_Icon, QHeaderView::Fixed);
    tableView->setColumnWidth(SC_Icon, kSessionsIconColumnWidth);
}
}

REGISTER_DOCK_WIDGET(SessionsTableWidget, "Sessions", true)

SessionsTableWidget::SessionsTableWidget( AdaptixWidget* w ) : DockTab("Sessions table", w->GetProfile()->GetProject(), ":/icons/format_list")
{
    this->adaptixWidget = w;

    this->createUI();

    connect( tableView, &QTreeView::doubleClicked,              this, &SessionsTableWidget::handleTableDoubleClicked );
    connect( tableView, &QTreeView::customContextMenuRequested, this, &SessionsTableWidget::handleSessionsTableMenu );
    connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &selected, const QItemSelection &deselected){
        Q_UNUSED(selected)
        Q_UNUSED(deselected)
        if (!inputFilter->hasFocus())
            tableView->setFocus();
    });

    connect(inputFilter,     &QLineEdit::textChanged,        this, &SessionsTableWidget::onFilterChanged);
    connect(inputFilter,     &QLineEdit::returnPressed,      this, [this]() { proxyModel->setTextFilter(inputFilter->text()); });
    connect(comboAgentType,  &QComboBox::currentTextChanged, this, &SessionsTableWidget::onFilterChanged);
    connect(checkOnlyActive, &QToolButton::toggled,          this, &SessionsTableWidget::onFilterChanged);
    connect(hideButton,      &ClickableLabel::clicked,       this, &SessionsTableWidget::toggleSearchPanel);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), this);
    shortcutSearch->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &SessionsTableWidget::toggleSearchPanel);

    auto shortcutEsc = new QShortcut(QKeySequence(Qt::Key_Escape), inputFilter);
    shortcutEsc->setContext(Qt::WidgetShortcut);
    connect(shortcutEsc, &QShortcut::activated, this, [this]() { searchWidget->setVisible(false); });

    auto shortcutProcessBrowser = new QShortcut(QKeySequence("Ctrl+P"), this);
    shortcutProcessBrowser->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutProcessBrowser, &QShortcut::activated, this, [this]() {
        QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
        for (const QModelIndex &proxyIndex : selectedRows) {
            if (groupingModel->isGroupIndex(proxyIndex))
                continue;
            qint64 agentId = groupingModel->agentIdFromIndex(proxyIndex);
            if (agentId == 0)
                continue;
            adaptixWidget->LoadProcessBrowserUI(agentId);
        }
    });

    auto shortcutFileBrowser = new QShortcut(QKeySequence("Ctrl+L"), this);
    shortcutFileBrowser->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutFileBrowser, &QShortcut::activated, this, [this]() {
        QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
        for (const QModelIndex &proxyIndex : selectedRows) {
            if (groupingModel->isGroupIndex(proxyIndex))
                continue;
            qint64 agentId = groupingModel->agentIdFromIndex(proxyIndex);
            if (agentId == 0)
                continue;
            adaptixWidget->LoadFileBrowserUI(agentId);
        }
    });

    auto shortcutTerminal = new QShortcut(QKeySequence("Ctrl+T"), this);
    shortcutTerminal->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutTerminal, &QShortcut::activated, this, [this]() {
        QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
        for (const QModelIndex &proxyIndex : selectedRows) {
            if (groupingModel->isGroupIndex(proxyIndex))
                continue;
            qint64 agentId = groupingModel->agentIdFromIndex(proxyIndex);
            if (agentId == 0)
                continue;
            adaptixWidget->LoadTerminalUI(agentId);
        }
    });

    auto shortcutConsole = new QShortcut(QKeySequence("Ctrl+I"), this);
    shortcutConsole->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutConsole, &QShortcut::activated, this, [this]() {
        QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
        for (const QModelIndex &proxyIndex : selectedRows) {
            if (groupingModel->isGroupIndex(proxyIndex))
                continue;
            qint64 agentId = groupingModel->agentIdFromIndex(proxyIndex);
            if (agentId == 0)
                continue;
            adaptixWidget->LoadConsoleUI(agentId);
        }
    });

    this->dockWidget->setWidget(this);
}

SessionsTableWidget::~SessionsTableWidget() = default;

void SessionsTableWidget::SetUpdatesEnabled(const bool enabled)
{
    if (!enabled) {
        bufferingEnabled = true;
    } else {
        bufferingEnabled = false;
        flushPendingAgents();
    }

    if (proxyModel) {
        proxyModel->setDynamicSortFilter(enabled);
        if (enabled)
            proxyModel->invalidate();
    }
    if (tableView) {
        tableView->setSortingEnabled(enabled);
        tableView->setUpdatesEnabled(enabled);
    }
    if (enabled) {
        if (agentsModel)
            agentsModel->refreshAll();
        if (tableView && tableView->viewport())
            tableView->viewport()->repaint();
        QTimer::singleShot(0, this, [this]() {
            if (tableView && tableView->viewport())
                tableView->viewport()->repaint();
            this->AutoFitColumnToContents(SC_AgentID);
        });
    }
}

void SessionsTableWidget::flushPendingAgents()
{
    if (pendingAgents.isEmpty())
        return;

    QList<qint64> ids;
    {
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        for (Agent* agent : pendingAgents) {
            if (adaptixWidget->AgentsMap.contains(agent->data.Id))
                ids.append(agent->data.Id);
        }
    }

    if (!ids.isEmpty())
        agentsModel->add(ids);

    pendingAgents.clear();

    if (!columnsSizedOnce && !ids.isEmpty()) {
        this->UpdateColumnsSize();
        columnsSizedOnce = true;
    }
}

void SessionsTableWidget::createUI()
{
    auto horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    searchWidget = new QWidget(this);
    searchWidget->setVisible(false);

    inputFilter = new oclero::qlementine::LineEdit(searchWidget);
    inputFilter->setIcon(QIcon(":/icons/search"));
    inputFilter->setPlaceholderText("filter: (admin | root) & ^(test)");
    inputFilter->setMaximumWidth(300);

    autoSearchCheck = new QCheckBox("auto", searchWidget);
    autoSearchCheck->setChecked(true);
    autoSearchCheck->setToolTip("Auto search on text change. If unchecked, press Enter to search.");

    comboAgentType = new QComboBox(searchWidget);
    comboAgentType->setMinimumWidth(150);
    comboAgentType->addItem("All types");

    checkOnlyActive = new QToolButton(searchWidget);
    checkOnlyActive->setCheckable(true);
    checkOnlyActive->setAutoRaise(true);
    checkOnlyActive->setCursor(Qt::PointingHandCursor);
    checkOnlyActive->setFocusPolicy(Qt::NoFocus);
    checkOnlyActive->setToolTip(QStringLiteral("Active only"));
    {
        const int h = FontManager::instance().typography().controlHeight;
        checkOnlyActive->setFixedSize(h, h);
        checkOnlyActive->setIconSize(QSize(qMax(14, h - 10), qMax(14, h - 10)));
    }
    checkOnlyActive->setChecked(GlobalClient->settings->data.SessionsAutoHideInactive);
    auto updateActiveIcon = [this]() {
        checkOnlyActive->setIcon(QIcon(checkOnlyActive->isChecked() ? QStringLiteral(":/icons/visibility_off") : QStringLiteral(":/icons/visibility")));
    };
    updateActiveIcon();
    connect(checkOnlyActive, &QToolButton::toggled, this, [updateActiveIcon](bool) { updateActiveIcon(); });
    checkOnlyActive->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(checkOnlyActive, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        oclero::qlementine::Menu menu(checkOnlyActive);
        menu.addAction(QIcon(QStringLiteral(":/icons/visibility")), QStringLiteral("Show all hidden"), this, &SessionsTableWidget::actionItemsShowAll);
        menu.exec(checkOnlyActive->mapToGlobal(pos));
    });

    hideButton = new ClickableLabel("  x  ");
    hideButton->setCursor(Qt::PointingHandCursor);

    searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(0, 4, 0, 0);
    searchLayout->setSpacing(4);
    searchLayout->addWidget(inputFilter);
    searchLayout->addWidget(autoSearchCheck);
    searchLayout->addSpacing(8);
    searchLayout->addWidget(comboAgentType);
    searchLayout->addSpacing(8);
    searchLayout->addWidget(checkOnlyActive);
    searchLayout->addSpacing(8);
    searchLayout->addWidget(hideButton);
    searchLayout->addSpacerItem(horizontalSpacer);

    agentsModel = new AgentsTableModel(adaptixWidget, this);
    proxyModel  = new AgentsFilterProxyModel(adaptixWidget, this);
    proxyModel->setSourceModel(agentsModel);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    groupingModel = new GroupingProxyModel(adaptixWidget, "agents", this);
    groupingModel->setSourceModel(proxyModel);

    tableView = new ColorAwareTreeView( this );
    tableView->setModel(groupingModel);
    tableView->setHeader(new BoldHeaderView(Qt::Horizontal, tableView));
    tableView->setContextMenuPolicy( Qt::CustomContextMenu );
    tableView->setAutoFillBackground( false );
    tableView->setSortingEnabled( true );
    tableView->setWordWrap( false );
    tableView->setSelectionBehavior( QAbstractItemView::SelectRows );
    tableView->setFocusPolicy( Qt::NoFocus );
    tableView->setAlternatingRowColors( true );
    tableView->setAnimated( true );
    tableView->setIndentation( 22 );
    tableView->setRootIsDecorated( true );
    tableView->setUniformRowHeights( true );
    tableView->setIndentGuides( true );
    tableView->setTreePosition( SC_AgentID );

    tableView->viewport()->setAutoFillBackground(true);
    {
        QPalette vp = tableView->viewport()->palette();
        vp.setColor(QPalette::Window, vp.color(QPalette::Base));
        tableView->viewport()->setPalette(vp);
    }
    tableView->header()->setSectionResizeMode( QHeaderView::Stretch );
    applySessionsIconColumnSize(tableView);
    tableView->header()->setSectionResizeMode( SC_AgentID, QHeaderView::ResizeToContents );
    tableView->header()->setCascadingSectionResizes( true );
    tableView->header()->setHighlightSections( false );
    tableView->header()->setSectionsMovable( true );
    tableView->header()->setFirstSectionMovable( false );

    columnStateReady = false;

    connect(tableView->header(), &QHeaderView::sectionMoved, this, [this](int, int) {
        if (!columnStateReady)
            return;
        this->SaveColumnOrder();
    });

    auto* header = tableView->header();
    header->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header, &QWidget::customContextMenuRequested, this, [this, header](const QPoint &pos) {
        const int logical = header->logicalIndexAt(pos);
        if (logical < 0 || logical >= SC_ColumnCount)
            return;

        oclero::qlementine::Menu menu(this);
        QAction* actAutoFit = menu.addAction("Auto fit this column");
        QAction* chosen = menu.exec(header->mapToGlobal(pos));
        if (chosen == actAutoFit)
            this->AutoFitColumnToContents(logical);
    });

    tableView->sortByColumn(SC_Created, Qt::AscendingOrder);

    auto* sessionsDelegate = new PaddingDelegate(tableView);
    tableView->setItemDelegate(sessionsDelegate);

    connect(groupingModel, &QAbstractItemModel::layoutChanged, this, [this]() {
        QTimer::singleShot(0, this, [this]() { this->AutoFitColumnToContents(SC_AgentID); });
    });
    connect(groupingModel, &QAbstractItemModel::modelReset, this, [this]() {
        QTimer::singleShot(0, this, [this]() { this->AutoFitColumnToContents(SC_AgentID); });
    });

    this->UpdateColumnsVisible();
    this->RestoreColumnState();
    QTimer::singleShot(0, this, [this]() {
        columnStateReady = true;
    });

    comboGroupBy = new QComboBox(searchWidget);
    comboGroupBy->setMinimumWidth(140);
    comboGroupBy->addItem("No grouping");
    comboGroupBy->addItem("By Domain");
    comboGroupBy->addItem("By Computer");
    comboGroupBy->addItem("By User");
    comboGroupBy->addItem("By Tag");
    comboGroupBy->addItem("By Listener");
    comboGroupBy->addItem("By OS");
    comboGroupBy->addItem("By Agent Type");
    comboGroupBy->addItem("Custom Groups");
    comboGroupBy->addItem("Pivot Tree");
    searchLayout->addSpacing(8);
    searchLayout->addWidget(comboGroupBy);

    connect(comboGroupBy, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SessionsTableWidget::onGroupModeChanged);

    btnGroupManager = new QPushButton(searchWidget);
    btnGroupManager->setIcon(this->style()->standardIcon(QStyle::SP_DirIcon));
    btnGroupManager->setToolTip("Manage custom groups");
    btnGroupManager->setFixedSize(28, 28);
    searchLayout->addSpacing(4);
    searchLayout->addWidget(btnGroupManager);

    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    tableView->setDragDropMode(QAbstractItemView::DragDrop);
    tableView->setDefaultDropAction(Qt::MoveAction);
    tableView->setDragEnabled(true);
    tableView->setAcceptDrops(true);
    tableView->setDropIndicatorShown(true);

    groupPopup = new GroupManagerPopup(groupingModel, adaptixWidget, "agents", btnGroupManager, this);
    connect(btnGroupManager, &QPushButton::clicked, groupPopup, &GroupManagerPopup::showPopup);
    connect(groupingModel, &GroupingProxyModel::groupStructureChanged, groupPopup, &GroupManagerPopup::Rebuild);
    connect(groupingModel, &GroupingProxyModel::agentsDroppedOnGroup, this,
        [this](const QList<QPair<qint64,qint64>>& moves, qint64 toGroupId) {
            AuthProfile* profile = adaptixWidget->GetProfile();
            if (!profile)
                return;
            const int64_t to = (toGroupId < 0) ? 0 : static_cast<int64_t>(toGroupId);
            for (const auto& [agentId, fromGroupId] : moves) {
                const int64_t from = (fromGroupId < 0) ? 0 : static_cast<int64_t>(fromGroupId);
                HttpReqGroupMoveMemberAsync(agentId, from, to, *profile, [](bool ok, const QString& message, const QJsonObject&) {
                    if (!ok)
                        MessageError(message.isEmpty() ? QStringLiteral("Failed to move agent between groups") : message);
                });
            }
        });
    connect(groupingModel, &GroupingProxyModel::groupReparented, this,
        [this](int64_t groupId, int64_t newParentId) {
            AuthProfile* profile = adaptixWidget->GetProfile();
            if (!profile)
                return;
            const int64_t parent = (newParentId < 0) ? 0 : newParentId;
            HttpReqGroupReparentAsync(groupId, parent, *profile, [](bool ok, const QString& message, const QJsonObject&) {
                if (!ok)
                    MessageError(message.isEmpty() ? QStringLiteral("Failed to reparent group") : message);
            });
        });

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->setContentsMargins( 0, 0,  0, 0);
    mainGridLayout->addWidget( searchWidget, 0, 0, 1, 1);
    mainGridLayout->addWidget( tableView,    1, 0, 1, 1);
}

void SessionsTableWidget::AddAgentItem( Agent* newAgent )
{
    if (bufferingEnabled) {
        pendingAgents.append(newAgent);
        return;
    }

    agentsModel->add(newAgent->data.Id);

    if (adaptixWidget->IsSynchronized()) {
        this->UpdateColumnsSize();
        columnsSizedOnce = true;
    }
}

void SessionsTableWidget::UpdateAgentItem(const AgentData &oldDatam, const Agent* agent)
{
    const AgentData& n = agent->data;
    const qint64 id = n.Id;

    if (oldDatam.Mark != n.Mark || oldDatam.Color != n.Color) {
        agentsModel->update(id);
    } else {
        if (oldDatam.ExternalIP   != n.ExternalIP)   agentsModel->updateColumn(id, SC_External);
        if (oldDatam.Listener     != n.Listener)     agentsModel->updateColumn(id, SC_Listener);
        if (oldDatam.InternalIP   != n.InternalIP)   agentsModel->updateColumn(id, SC_Internal);
        if (oldDatam.Domain       != n.Domain)       agentsModel->updateColumn(id, SC_Domain);
        if (oldDatam.Computer     != n.Computer)     agentsModel->updateColumn(id, SC_Computer);
        if (oldDatam.Username != n.Username || oldDatam.Elevated != n.Elevated || oldDatam.Impersonated != n.Impersonated) agentsModel->updateColumn(id, SC_User);
        if (oldDatam.OsDesc       != n.OsDesc)       agentsModel->updateColumn(id, SC_Os);
        if (oldDatam.Process != n.Process || oldDatam.Arch != n.Arch) agentsModel->updateColumn(id, SC_Process);
        if (oldDatam.Pid          != n.Pid)          agentsModel->updateColumn(id, SC_Pid);
        if (oldDatam.Tid          != n.Tid)          agentsModel->updateColumn(id, SC_Tid);
        if (oldDatam.Tags         != n.Tags)         agentsModel->updateColumn(id, SC_Tags);
        if (oldDatam.Sleep != n.Sleep || oldDatam.Jitter != n.Jitter || oldDatam.Async != n.Async) agentsModel->updateColumn(id, SC_Sleep);
        if (oldDatam.LastTick     != n.LastTick)     agentsModel->updateColumn(id, SC_Last);
    }

    if (oldDatam.Username != n.Username || oldDatam.Impersonated != n.Impersonated || oldDatam.Elevated != n.Elevated)
        tableView->resizeColumnToContents(SC_User);
    if (oldDatam.Domain    != n.Domain)   tableView->resizeColumnToContents(SC_Domain);
    if (oldDatam.Computer  != n.Computer) tableView->resizeColumnToContents(SC_Computer);
    if (oldDatam.OsDesc    != n.OsDesc)   tableView->resizeColumnToContents(SC_Os);
    if (oldDatam.Process   != n.Process || oldDatam.Arch != n.Arch)
        tableView->resizeColumnToContents(SC_Process);
}

void SessionsTableWidget::RemoveAgentItem(qint64 agentId)
{
    if (groupingModel)
        groupingModel->removeAgentFromAllCustomGroups(agentId);
    if (agentsModel)
        agentsModel->remove(agentId);
}

void SessionsTableWidget::UpdateColumnsVisible()
{
    if (!tableView || !GlobalClient || !GlobalClient->settings)
        return;
    for (int i = 0; i < SC_ColumnCount; i++) {
        if (GlobalClient->settings->data.SessionsTableColumns[i])
            tableView->showColumn(i);
        else
            tableView->hideColumn(i);
    }
    if (agentsModel)
        agentsModel->refreshAll();
}

void SessionsTableWidget::UpdateColumnsSize()
{
    tableView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tableView->header()->setSectionResizeMode(SC_Tags, QHeaderView::Stretch);

    int wDomain   = tableView->columnWidth(SC_Domain);
    int wComputer = tableView->columnWidth(SC_Computer);
    int wUser     = tableView->columnWidth(SC_User);
    int wOs       = tableView->columnWidth(SC_Os);
    int wProcess  = tableView->columnWidth(SC_Process);

    tableView->header()->setSectionResizeMode(SC_Domain,   QHeaderView::Interactive);
    tableView->header()->setSectionResizeMode(SC_Computer, QHeaderView::Interactive);
    tableView->header()->setSectionResizeMode(SC_User,     QHeaderView::Interactive);
    tableView->header()->setSectionResizeMode(SC_Os,       QHeaderView::Interactive);
    tableView->header()->setSectionResizeMode(SC_Process,  QHeaderView::Interactive);

    tableView->setColumnWidth(SC_Domain,   wDomain);
    tableView->setColumnWidth(SC_Computer, wComputer);
    tableView->setColumnWidth(SC_User,     wUser);
    tableView->setColumnWidth(SC_Os,       wOs);
    tableView->setColumnWidth(SC_Process,  wProcess);

    tableView->header()->setSectionResizeMode(SC_AgentID, QHeaderView::Interactive);
    tableView->resizeColumnToContents(SC_AgentID);

    applySessionsIconColumnSize(tableView);
}

void SessionsTableWidget::UpdateData()
{
    auto f = qobject_cast<AgentsFilterProxyModel*>(proxyModel);
    f->updateVisible();
}

void SessionsTableWidget::UpdateLastColumn(const QList<qint64>& agentIds)
{
    agentsModel->updateLastColumn(agentIds);
}

void SessionsTableWidget::UpdateAgentTypeComboBox()
{
    QSet<QString> types;
    {
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        for (const auto& agent : adaptixWidget->AgentsMap) {
            if (agent && !agent->data.Name.isEmpty())
                types.insert(agent->data.Name);
        }
    }

    QString currentType = comboAgentType->currentText();

    comboAgentType->blockSignals(true);
    comboAgentType->clear();
    comboAgentType->addItem("All types");
    QStringList typeList = types.values();
    typeList.sort();
    comboAgentType->addItems(typeList);

    int idx = comboAgentType->findText(currentType);
    if (idx >= 0)
        comboAgentType->setCurrentIndex(idx);
    comboAgentType->blockSignals(false);
}

void SessionsTableWidget::Clear()
{
    if (agentsModel)
        agentsModel->clear();
    if (groupingModel)
        groupingModel->clearCustomGroups();
    columnsSizedOnce = false;
    pendingAgents.clear();

    if (checkOnlyActive)
        checkOnlyActive->setChecked(false);
    if (inputFilter)
        inputFilter->clear();
    if (comboAgentType) {
        comboAgentType->clear();
        comboAgentType->addItem("All types");
    }
    if (comboGroupBy)
        comboGroupBy->setCurrentIndex(0);
}



/// SLOTS

void SessionsTableWidget::toggleSearchPanel() const
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

void SessionsTableWidget::handleTableDoubleClicked(const QModelIndex &index) const
{
    if (!index.isValid())
        return;

    if (groupingModel->isGroupIndex(index)) {
        if (tableView->isExpanded(index))
            tableView->collapse(index);
        else
            tableView->expand(index);
        return;
    }

    qint64 AgentId = groupingModel->agentIdFromIndex(index);
    if (AgentId == 0)
        return;

    adaptixWidget->LoadConsoleUI(AgentId);
}

void SessionsTableWidget::onFilterChanged() const
{
    if (autoSearchCheck->isChecked()) {
        proxyModel->setTextFilter(inputFilter->text());
    }
    proxyModel->setOnlyActive(checkOnlyActive->isChecked());

    QSet<QString> selectedTypes;
    QString currentType = comboAgentType->currentText();
    if (currentType != "All types" && !currentType.isEmpty()) {
        selectedTypes.insert(currentType);
    }
    proxyModel->setAgentTypes(selectedTypes);
}

/// Menu

void SessionsTableWidget::handleSessionsTableMenu(const QPoint &pos)
{
    oclero::qlementine::Menu ctxMenu;

    QModelIndex index = tableView->indexAt(pos);
    if (index.isValid() && !groupingModel->isGroupIndex(index)) {
        QList<qint64> agentIds;
        QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
        for (const QModelIndex &proxyIndex : selectedRows) {
            if (groupingModel->isGroupIndex(proxyIndex))
                continue;
            qint64 agentId = groupingModel->agentIdFromIndex(proxyIndex);
            if (agentId == 0)
                continue;
            agentIds.append(agentId);
        }


        ctxMenu.addAction(QIcon(QStringLiteral(":/icons/interact")), QStringLiteral("Interact"), this, &SessionsTableWidget::actionConsoleOpen);
        ctxMenu.addSeparator();

        auto* exploreMenu = ctxMenu.addMenu(QIcon(QStringLiteral(":/icons/open_folder")), QStringLiteral("Explore"));
        int browserCount = adaptixWidget->ScriptManager->AddMenuSession(exploreMenu, QStringLiteral("SessionBrowser"), agentIds);
        if (browserCount > 0)
            ctxMenu.addMenu(exploreMenu);
        else
            ctxMenu.removeAction(exploreMenu->menuAction());

        auto* accessMenu = ctxMenu.addMenu(QIcon(QStringLiteral(":/icons/exchange")), QStringLiteral("Access"));
        int accessCount = adaptixWidget->ScriptManager->AddMenuSession(accessMenu, QStringLiteral("SessionAccess"), agentIds);
        if (accessCount > 0)
            ctxMenu.addMenu(accessMenu);
        else
            ctxMenu.removeAction(accessMenu->menuAction());

        auto* tasksMenu = ctxMenu.addMenu(QIcon(QStringLiteral(":/icons/job")), QStringLiteral("Tasks"));
        tasksMenu->addAction(QIcon(QStringLiteral(":/icons/keyboard_command")), QStringLiteral("Execute command"), this, &SessionsTableWidget::actionExecuteCommand);
        tasksMenu->addAction(QIcon(QStringLiteral(":/icons/job")), QStringLiteral("Task manager"), this, &SessionsTableWidget::actionTasksBrowserOpen);
        adaptixWidget->ScriptManager->AddMenuSession(tasksMenu, QStringLiteral("SessionAgent"), agentIds);

        auto* extMenu = ctxMenu.addMenu(QIcon(QStringLiteral(":/icons/extension")), QStringLiteral("Extensions"));
        int mainCount = adaptixWidget->ScriptManager->AddMenuSession(extMenu, QStringLiteral("SessionMain"), agentIds);
        if (mainCount > 0)
            ctxMenu.addMenu(extMenu);
        else
            ctxMenu.removeAction(extMenu->menuAction());

        ctxMenu.addSeparator();

        ctxMenu.addAction(QIcon(QStringLiteral(":/icons/wifi_signal")), QStringLiteral("Mark as Active"), this, &SessionsTableWidget::actionMarkActive);
        ctxMenu.addAction(QIcon(QStringLiteral(":/icons/wifi_null")), QStringLiteral("Mark as Inactive"), this, &SessionsTableWidget::actionMarkInactive);
        ctxMenu.addAction(QIcon(QStringLiteral(":/icons/visibility_off")), QStringLiteral("Hide on client"), this, &SessionsTableWidget::actionItemHide);
        ctxMenu.addSeparator();

        ctxMenu.addAction(QIcon(QStringLiteral(":/icons/tag")), QStringLiteral("Set tag"), this, &SessionsTableWidget::actionItemTag);
        if (agentIds.size() == 1)
            ctxMenu.addAction(QIcon(QStringLiteral(":/icons/info")), QStringLiteral("Session info…"), this, &SessionsTableWidget::actionSetData);
        auto* appearanceMenu = ctxMenu.addMenu(QIcon(QStringLiteral(":/icons/picture")), QStringLiteral("Appearance"));
        appearanceMenu->addAction(QIcon(QStringLiteral(":/icons/color_fill")), QStringLiteral("Set items color"), this, &SessionsTableWidget::actionItemColor);
        appearanceMenu->addAction(QIcon(QStringLiteral(":/icons/color_text")), QStringLiteral("Set text color"), this, &SessionsTableWidget::actionTextColor);
        appearanceMenu->addAction(QIcon(QStringLiteral(":/icons/color_reset")), QStringLiteral("Reset color"), this, &SessionsTableWidget::actionColorReset);

        ctxMenu.addSeparator();
        ctxMenu.addAction(QIcon(QStringLiteral(":/icons/delete")), QStringLiteral("Remove from server"),
                          this, &SessionsTableWidget::actionAgentRemove);

        ctxMenu.exec(tableView->viewport()->mapToGlobal(pos));
    }
}

void SessionsTableWidget::actionConsoleOpen() const
{
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        if (groupingModel->isGroupIndex(proxyIndex))
            continue;
        qint64 agentId = groupingModel->agentIdFromIndex(proxyIndex);
        if (agentId == 0)
            continue;
        adaptixWidget->LoadConsoleUI(agentId);
    }
}

void SessionsTableWidget::actionExecuteCommand()
{
    QList<qint64> listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        if (groupingModel->isGroupIndex(proxyIndex))
            continue;
        qint64 agentId = groupingModel->agentIdFromIndex(proxyIndex);
        if (agentId == 0)
            continue;
        listId.append(agentId);
    }

    if(listId.empty())
        return;

    bool ok = false;
    QString cmd = QInputDialog::getText(this,"Execute Command", "Command", QLineEdit::Normal, "", &ok);
    if (!ok)
        return;

    QList<ConsoleWidget*> consoles;
    {
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        for(auto id : listId) {
            if (adaptixWidget->AgentsMap.contains(id))
                consoles.append(adaptixWidget->AgentsMap[id]->Console);
        }
    }
    for (auto* console : consoles) {
        console->SetInput(cmd);
        console->processInput();
    }
}

void SessionsTableWidget::actionTasksBrowserOpen() const
{
    auto idx = tableView->currentIndex();
    if (idx.isValid() && !groupingModel->isGroupIndex(idx)) {
        qint64 agentId = groupingModel->agentIdFromIndex(idx);
        if (agentId == 0)
            return;

        adaptixWidget->TasksDock->SetAgentFilter(agentId);
        adaptixWidget->SetTasksUI();
    }
}

void SessionsTableWidget::actionMarkActive() const
{
    QList<qint64> listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        if (groupingModel->isGroupIndex(proxyIndex))
            continue;
        qint64 agentId = groupingModel->agentIdFromIndex(proxyIndex);
        if (agentId == 0)
            continue;
        listId.append(agentId);
    }

    if(listId.empty())
        return;

    HttpReqAgentSetMarkAsync(listId, "", *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void SessionsTableWidget::actionMarkInactive() const
{
    QList<qint64> listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        if (groupingModel->isGroupIndex(proxyIndex))
            continue;
        qint64 agentId = groupingModel->agentIdFromIndex(proxyIndex);
        if (agentId == 0)
            continue;
        listId.append(agentId);
    }

    if(listId.empty())
        return;

    HttpReqAgentSetMarkAsync(listId, "Inactive", *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void SessionsTableWidget::actionItemColor() const
{
    QList<qint64> listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        if (groupingModel->isGroupIndex(proxyIndex))
            continue;
        qint64 agentId = groupingModel->agentIdFromIndex(proxyIndex);
        if (agentId == 0)
            continue;
        listId.append(agentId);
    }

    if(listId.empty())
        return;

    QColor itemColor = QColorDialog::getColor(Qt::white, nullptr, "Select items color");
    if (itemColor.isValid()) {
        QString itemColorHex = itemColor.name();
        HttpReqAgentSetColorAsync(listId, itemColorHex, "", false, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void SessionsTableWidget::actionTextColor() const
{
    QList<qint64> listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        if (groupingModel->isGroupIndex(proxyIndex))
            continue;
        qint64 agentId = groupingModel->agentIdFromIndex(proxyIndex);
        if (agentId == 0)
            continue;
        listId.append(agentId);
    }

    if(listId.empty())
        return;

    QColor textColor = QColorDialog::getColor(Qt::white, nullptr, "Select text color");
    if (textColor.isValid()) {
        QString textColorHex = textColor.name();
        HttpReqAgentSetColorAsync(listId, "", textColorHex, false, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void SessionsTableWidget::actionColorReset() const
{
    QList<qint64> listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        if (groupingModel->isGroupIndex(proxyIndex))
            continue;
        qint64 agentId = groupingModel->agentIdFromIndex(proxyIndex);
        if (agentId == 0)
            continue;
        listId.append(agentId);
    }

    if(listId.empty())
        return;

    HttpReqAgentSetColorAsync(listId, "", "", true, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void SessionsTableWidget::actionAgentRemove()
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Confirmation",
                                      "Are you sure you want to delete all information about the selected agents from the server?\n\n"
                                      "If you want to hide the record, simply choose: 'Item -> Hide on Client'.",
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    QList<qint64> listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        if (groupingModel->isGroupIndex(proxyIndex))
            continue;
        qint64 agentId = groupingModel->agentIdFromIndex(proxyIndex);
        if (agentId == 0)
            continue;
        listId.append(agentId);
    }

    if(listId.empty())
        return;

    HttpReqAgentRemoveAsync(listId, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void SessionsTableWidget::actionItemTag() const
{
    QString tag = "";
    QList<qint64> listId;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : selectedRows) {
        if (groupingModel->isGroupIndex(proxyIndex))
            continue;
        qint64 agentId = groupingModel->agentIdFromIndex(proxyIndex);
        if (agentId == 0)
            continue;
        listId.append(agentId);

        if (tag.isEmpty()) {
            QReadLocker locker(&adaptixWidget->AgentsMapLock);
            Agent* agent = adaptixWidget->AgentsMap.value(agentId, nullptr);
            if (agent)
                tag = agent->data.Tags;
        }
    }

    if(listId.empty())
        return;

    bool inputOk;
    QString newTag = QInputDialog::getText(nullptr, "Set tags", "New tag", QLineEdit::Normal,tag, &inputOk);
    if ( inputOk ) {
        HttpReqAgentSetTagAsync(listId, newTag, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void SessionsTableWidget::actionItemHide()
{
    bool refact = false;
    QModelIndexList selectedRows = tableView->selectionModel()->selectedRows();

    QReadLocker locker(&adaptixWidget->AgentsMapLock);
    for (const QModelIndex &proxyIndex : selectedRows) {
        if (groupingModel->isGroupIndex(proxyIndex))
            continue;
        qint64 agentId = groupingModel->agentIdFromIndex(proxyIndex);
        if (agentId == 0)
            continue;
        if (adaptixWidget->AgentsMap.contains(agentId)) {
            adaptixWidget->AgentsMap[agentId]->show = false;
            refact = true;
        }
    }
    locker.unlock();

    if (refact) this->UpdateData();
}

void SessionsTableWidget::actionItemsShowAll()
{
    bool refact = false;
    {
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        for (auto agent : adaptixWidget->AgentsMap) {
            if (agent->show == false) {
                agent->show = true;
                refact = true;
            }
        }
    }

    if (refact) this->UpdateData();
}

void SessionsTableWidget::actionSetData() const
{
    auto idx = tableView->currentIndex();
    if (!idx.isValid() || groupingModel->isGroupIndex(idx))
        return;

    qint64 agentId = groupingModel->agentIdFromIndex(idx);
    if (agentId == 0)
        return;

    AgentData agentData;
    {
        QReadLocker locker(&adaptixWidget->AgentsMapLock);
        if (!adaptixWidget->AgentsMap.contains(agentId))
            return;
        agentData = adaptixWidget->AgentsMap[agentId]->data;
    }

    auto* dialog = new DialogAgentData();
    dialog->SetProfile(*(adaptixWidget->GetProfile()));
    dialog->SetAgentData(agentData);
    dialog->Start();
}

void SessionsTableWidget::RestoreColumnState() const
{
    QHeaderView* header = tableView->header();

    for (int logicalIndex = 0; logicalIndex < SC_ColumnCount; logicalIndex++) {
        int savedVisualIndex = GlobalClient->settings->data.SessionsColumnOrder[logicalIndex];
        if (savedVisualIndex >= 0 && savedVisualIndex < SC_ColumnCount) {
            int currentVisualIndex = header->visualIndex(logicalIndex);
            if (currentVisualIndex != savedVisualIndex) {
                header->moveSection(currentVisualIndex, savedVisualIndex);
            }
        }
    }
}

void SessionsTableWidget::SaveColumnOrder() const
{
    QHeaderView* header = tableView->header();

    for (int logicalIndex = 0; logicalIndex < SC_ColumnCount; logicalIndex++) {
        GlobalClient->settings->data.SessionsColumnOrder[logicalIndex] = header->visualIndex(logicalIndex);
    }

    GlobalClient->settings->SaveToDB();
}

void SessionsTableWidget::AutoFitColumnToContents(const int logicalIndex) const
{
    if (!tableView || !groupingModel)
        return;

    if (logicalIndex < 0 || logicalIndex >= SC_ColumnCount)
        return;

    if (tableView->isColumnHidden(logicalIndex))
        return;

    if (logicalIndex == SC_Icon) {
        applySessionsIconColumnSize(tableView);
        return;
    }

    int maxWidth = 0;
    std::function<void(const QModelIndex&)> measureRecursive = [&](const QModelIndex& parent) {
        const int rows = groupingModel->rowCount(parent);
        for (int row = 0; row < rows; row++) {
            const QModelIndex idx = groupingModel->index(row, logicalIndex, parent);
            if (!idx.isValid())
                continue;
            int contentWidth = tableView->sizeHintForIndex(idx).width();
            maxWidth = qMax(maxWidth, contentWidth);
            if (groupingModel->hasChildren(idx))
                measureRecursive(idx);
        }
    };
    measureRecursive(QModelIndex());


    maxWidth += 24;
    if (maxWidth < 50)
        maxWidth = 50;

    tableView->header()->setSectionResizeMode(logicalIndex, QHeaderView::Interactive);
    tableView->setColumnWidth(logicalIndex, maxWidth);
}

void SessionsTableWidget::onGroupModeChanged(int index)
{
    switch (index) {
        case 0:  groupingModel->setViewMode(VM_Flat); break;
        case 1:  groupingModel->setViewMode(VM_AutoGroup); groupingModel->setAutoGroupField(AG_ByDomain); break;
        case 2:  groupingModel->setViewMode(VM_AutoGroup); groupingModel->setAutoGroupField(AG_ByComputer); break;
        case 3:  groupingModel->setViewMode(VM_AutoGroup); groupingModel->setAutoGroupField(AG_ByUser); break;
        case 4:  groupingModel->setViewMode(VM_AutoGroup); groupingModel->setAutoGroupField(AG_ByTag); break;
        case 5:  groupingModel->setViewMode(VM_AutoGroup); groupingModel->setAutoGroupField(AG_ByListener); break;
        case 6:  groupingModel->setViewMode(VM_AutoGroup); groupingModel->setAutoGroupField(AG_ByOs); break;
        case 7:  groupingModel->setViewMode(VM_AutoGroup); groupingModel->setAutoGroupField(AG_ByAgentType); break;
        case 8:  groupingModel->setViewMode(VM_CustomGroups); break;
        case 9:  groupingModel->setViewMode(VM_Pivots); break;
        default: groupingModel->setViewMode(VM_Flat); break;
    }

    tableView->expandAll();
    this->AutoFitColumnToContents(SC_AgentID);
}

void SessionsTableWidget::OnGroupCreated(int64_t groupId, int64_t parentId, const QString& name, const QVector<qint64>& members)
{
    if (groupingModel)
        groupingModel->addCustomGroup(groupId, parentId, name, members);
}

void SessionsTableWidget::OnGroupRenamed(int64_t groupId, const QString& name)
{
    if (groupingModel)
        groupingModel->renameCustomGroup(groupId, name);
}

void SessionsTableWidget::OnGroupDeleted(int64_t groupId)
{
    if (groupingModel)
        groupingModel->deleteCustomGroup(groupId);
}

void SessionsTableWidget::OnGroupMembersChanged(int64_t groupId, const QVector<qint64>& add, const QVector<qint64>& remove)
{
    if (groupingModel)
        groupingModel->setCustomGroupMembers(groupId, add, remove);
}

void SessionsTableWidget::OnGroupReparented(int64_t groupId, int64_t newParentId)
{
    if (groupingModel)
        groupingModel->reparentCustomGroup(groupId, newParentId);
}