#include <UI/Widgets/ScriptsWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <Client/AuthProfile.h>
#include <Client/Extender.h>
#include <Client/Requestor.h>
#include <Client/PagedTableHelper.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Utils/NonBlockingDialogs.h>
#include <Utils/FontManager.h>
#include <Utils/CustomElements/BoldHeaderView.h>
#include <Utils/CustomElements/ListFeed.h>
#include <UI/MainUI.h>
#include <MainAdaptix.h>

#include <oclero/qlementine/widgets/Menu.hpp>
#include <oclero/qlementine/widgets/SegmentedControl.hpp>
#include <oclero/qlementine/widgets/LineEdit.hpp>

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMessageBox>
#include <QPointer>
#include <QIcon>
#include <QComboBox>
#include <QLabel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QTableView>
#include <QHeaderView>
#include <QStandardItem>
#include <QDateTime>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QApplication>
#include <QTimer>
#include <QToolTip>
#include <QHelpEvent>
#include <QAbstractItemView>
#include <QEvent>
#include <QSet>
#include <QSignalBlocker>
#include <QListWidget>
#include <QSplitter>
#include <QListWidgetItem>
#include <QFrame>

REGISTER_DOCK_WIDGET(ScriptsWidget, "Scripts", false)

static constexpr int kIdRole       = Qt::UserRole + 1;
static constexpr int kStatusRole   = Qt::UserRole + 2;
static constexpr int kElideMiddleRole = Qt::UserRole + 3;
static constexpr int kKindRole     = Qt::UserRole + 4; // "local" | "server"

bool ScriptsTableFilter::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    Q_UNUSED(sourceParent)
    auto* model = qobject_cast<QStandardItemModel*>(sourceModel());
    if (!model)
        return true;

    if (!m_originFilter.isEmpty()) {
        QStandardItem* kindIt = model->item(sourceRow, 0);
        const QString kind = kindIt ? kindIt->data(kKindRole).toString() : QString();
        if (kind != m_originFilter)
            return false;
    }

    if (m_searchText.isEmpty())
        return true;
    const QString lower = m_searchText.toLower();
    for (int c = 0; c < model->columnCount(); ++c) {
        QStandardItem* it = model->item(sourceRow, c);
        if (it && it->text().toLower().contains(lower))
            return true;
    }
    return false;
}

static QStandardItem* idItem(const QString& id, const QString& display)
{
    auto* it = new QStandardItem(display);
    it->setData(id, kIdRole);
    it->setEditable(false);
    if (!display.isEmpty())
        it->setToolTip(display);
    return it;
}

static QStandardItem* textItem(const QString& t)
{
    auto* it = new QStandardItem(t);
    it->setEditable(false);
    if (!t.isEmpty())
        it->setToolTip(t);
    return it;
}

static QStandardItem* nameItem(const QString& t)
{
    auto* it = textItem(t);
    QFont f = it->font();
    f.setBold(true);
    it->setFont(f);
    const FeedColors fc = FeedColors::fromTheme();
    it->setForeground(fc.textPrimary);
    return it;
}

static QStandardItem* mutedItem(const QString& t, bool elideMiddle = false)
{
    auto* it = textItem(t);
    const FeedColors fc = FeedColors::fromTheme();
    it->setForeground(fc.textSecondary);
    if (elideMiddle)
        it->setData(true, kElideMiddleRole);
    return it;
}

static QStandardItem* statusItem(const QString& id, const QString& status, const QString& kind)
{
    auto* it = idItem(id, status);
    it->setData(status, kStatusRole);
    it->setData(kind, kKindRole);
    it->setTextAlignment(Qt::AlignCenter);
    return it;
}

class ScriptsTableDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const bool selected = opt.state & QStyle::State_Selected;
        paintFeedTableCellBackground(painter, opt.rect, selected, /*hovered=*/false, index.row() % 2 == 1, index.column() == 0);

        if (index.column() == 0 && index.data(kStatusRole).isValid()) {
            const QString status = index.data(kStatusRole).toString();
            const FeedColors fc = FeedColors::fromTheme();
            QColor pen = fc.textSecondary;
            if (status == QLatin1String("Enabled") || status == QLatin1String("ON"))
                pen = fc.success;
            else if (status == QLatin1String("Error") || status == QLatin1String("ERR"))
                pen = fc.error;
            else if (status == QLatin1String("Muted") || status == QLatin1String("MUTED"))
                pen = fc.canceled.isValid() ? fc.canceled : fc.running;
            else if (status == QLatin1String("Disabled") || status == QLatin1String("OFF"))
                pen = fc.textSecondary;
            QFont f = opt.font;
            f.setBold(true);
            painter->setFont(f);
            painter->setPen(pen);
            QRect textRect = opt.rect.adjusted(12, 0, -8, 0);
            painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, status);
        } else {
            QRect textRect = opt.rect.adjusted(index.column() == 0 ? 12 : 8, 0, -8, 0);
            QColor pen = opt.palette.color(QPalette::Text);
            const QVariant fg = index.data(Qt::ForegroundRole);
            if (fg.isValid())
                pen = fg.value<QColor>();
            painter->setPen(pen);
            painter->setFont(opt.font);
            const bool mid = index.data(kElideMiddleRole).toBool();
            const QString elided = opt.fontMetrics.elidedText(
                opt.text, mid ? Qt::ElideMiddle : Qt::ElideRight, textRect.width());
            painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elided);
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        QString text = opt.text;
        QFont f = opt.font;
        int hPad = 16; // left+right padding used in paint
        if (index.column() == 0 && index.data(kStatusRole).isValid()) {
            text = index.data(kStatusRole).toString();
            f.setBold(true);
            hPad = 20; // 12 + 8
        } else if (index.column() == 0) {
            hPad = 20;
        }

        const QFontMetrics fm(f);
        const int textW = fm.horizontalAdvance(text);
        const int w = qMax(48, textW + hPad + 4);
        const int h = qMax(fm.height() + 10, 30);
        return QSize(w, h);
    }

    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option, const QModelIndex& index) override
    {
        if (event && event->type() == QEvent::ToolTip) {
            const QString tip = index.data(Qt::ToolTipRole).toString();
            if (!tip.isEmpty()) {
                QToolTip::showText(event->globalPos(), tip, view);
                return true;
            }
        }
        return QStyledItemDelegate::helpEvent(event, view, option, index);
    }
};

enum class ScriptsTableKind { Scripts, Events };

static void fitContentColumnsThenStretchLast(QTableView* tv)
{
    if (!tv || !tv->model() || !tv->horizontalHeader())
        return;
    auto* h = tv->horizontalHeader();
    const int n = tv->model()->columnCount();
    if (n <= 0)
        return;

    h->setStretchLastSection(false);
    h->setMinimumSectionSize(48);

    for (int c = 0; c < n; ++c)
        h->setSectionResizeMode(c, QHeaderView::ResizeToContents);

    tv->resizeColumnsToContents();

    for (int c = 0; c < n - 1; ++c) {
        const QString label = tv->model()->headerData(c, Qt::Horizontal).toString();
        const int headerW = h->fontMetrics().horizontalAdvance(label) + 28; // padding + sort indicator
        const int contentW = h->sectionSize(c);
        const int w = qMax(contentW, headerW);
        h->setSectionResizeMode(c, QHeaderView::Interactive);
        h->resizeSection(c, w);
    }
    if (n > 0)
        h->setSectionResizeMode(n - 1, QHeaderView::Stretch);
}

static void applyScriptsColumnLayout(QTableView* tv, ScriptsTableKind kind)
{
    if (!tv || !tv->horizontalHeader())
        return;
    auto* hdr = tv->horizontalHeader();
    const int cols = tv->model() ? tv->model()->columnCount() : 0;
    if (cols <= 0)
        return;

    hdr->setStretchLastSection(false);
    hdr->setHighlightSections(false);
    hdr->setSectionsClickable(true);
    hdr->setMinimumSectionSize(48);

    if (kind == ScriptsTableKind::Scripts) {
        fitContentColumnsThenStretchLast(tv);
        if (hdr->count() > 1 && hdr->sectionSize(1) < 140)
            hdr->resizeSection(1, 140);
    } else {
        fitContentColumnsThenStretchLast(tv);
    }
}

static void rebalanceScriptsColumns(QTableView* tv, ScriptsTableKind kind)
{
    if (!tv || !tv->horizontalHeader())
        return;
    if (tv->viewport()->width() < 100 || tv->horizontalHeader()->count() <= 0)
        return;
    Q_UNUSED(kind);
    fitContentColumnsThenStretchLast(tv);
}

static EventHandlerInfo parseEventHandlerFromJson(const QJsonObject& o)
{
    EventHandlerInfo info;
    info.id = o.value(QStringLiteral("id")).toString();
    info.name = o.value(QStringLiteral("name")).toString();
    info.description = o.value(QStringLiteral("description")).toString();
    info.group = o.value(QStringLiteral("group")).toString();
    info.event = o.value(QStringLiteral("event")).toString();
    info.source = o.value(QStringLiteral("source")).toString();
    info.enabled = o.value(QStringLiteral("enabled")).toBool();
    info.eventMuted = o.value(QStringLiteral("event_muted")).toBool();
    info.lastError = o.value(QStringLiteral("last_error")).toString();
    info.lastRunAt = static_cast<qint64>(o.value(QStringLiteral("last_run_at")).toDouble());
    if (o.value(QStringLiteral("filters")).isObject())
        info.filters = o.value(QStringLiteral("filters")).toObject();
    return info;
}

static QString filtersTooltip(const QJsonObject& filters)
{
    if (filters.isEmpty())
        return QStringLiteral("filters: (none)");
    return QStringLiteral("filters: %1").arg(QString::fromUtf8(QJsonDocument(filters).toJson(QJsonDocument::Compact)));
}

static QString scriptLocationLabel(const ServerScriptInfo& info)
{
    return QStringLiteral("teamserver · %1").arg(info.name);
}

static void styleToolbarSearch(QLineEdit* edit)
{
    if (!edit)
        return;
    edit->setClearButtonEnabled(true);
    edit->setMinimumWidth(180);
    edit->setMaximumWidth(420);
    edit->setFixedHeight(FontManager::instance().typography().controlHeight);
    if (auto* le = qobject_cast<oclero::qlementine::LineEdit*>(edit))
        le->setIcon(QIcon(QStringLiteral(":/icons/search")));
}

static QLabel* makeEmptyLabel(QWidget* parent, const QString& text)
{
    auto* lab = new QLabel(text, parent);
    lab->setAlignment(Qt::AlignCenter);
    lab->setWordWrap(true);
    const FeedColors fc = FeedColors::fromTheme();
    QPalette pal = lab->palette();
    pal.setColor(QPalette::WindowText, fc.textSecondary);
    lab->setPalette(pal);
    QFont f = lab->font();
    f.setPointSizeF(qMax(9.0, f.pointSizeF() > 0 ? f.pointSizeF() + 0.5 : 11.0));
    lab->setFont(f);
    lab->setVisible(false);
    lab->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    return lab;
}

static void styleTable(QTableView* tv)
{
    tv->setSelectionBehavior(QAbstractItemView::SelectRows);
    tv->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tv->setShowGrid(false);
    tv->setSortingEnabled(true);
    tv->setWordWrap(false);
    tv->setContextMenuPolicy(Qt::CustomContextMenu);
    tv->setFocusPolicy(Qt::NoFocus);
    tv->setMouseTracking(true);
    tv->verticalHeader()->setVisible(false);
    tv->verticalHeader()->setDefaultSectionSize(FontManager::instance().typography().rowHeightCompact);
    auto* hdr = new BoldHeaderView(Qt::Horizontal, tv);
    tv->setHorizontalHeader(hdr);
    tv->setItemDelegate(new ScriptsTableDelegate(tv));
    applyFeedTableViewChrome(tv);
}

ScriptsWidget::ScriptsWidget(AdaptixWidget* w) : DockTab("Scripts", w->GetProfile()->GetProject(), ":/icons/folder_code"), adaptixWidget(w)
{
    applyFeedWidgetSurface(this);

    setupScriptsTable();
    setupEventsTable();

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_scriptsPanel);
    m_stack->addWidget(m_eventsPanel);
    m_stack->setCurrentIndex(0);
    applyFeedWidgetSurface(m_stack);
    applyFeedWidgetSurface(m_scriptsPanel);
    applyFeedWidgetSurface(m_eventsPanel);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_stack, 1);

    auto makeSegment = [this](QWidget* panel, int idx) {
        auto* seg = new oclero::qlementine::SegmentedControl(panel);
        seg->addItem(QStringLiteral("Scripts"));
        seg->addItem(QStringLiteral("Events"));
        seg->setCurrentIndex(idx);
        seg->setFixedHeight(FontManager::instance().typography().segmentHeight);
        return seg;
    };

    m_segScripts = makeSegment(m_scriptsPanel, 0);
    m_segEvents  = makeSegment(m_eventsPanel, 1);

    auto insertSeg = [](QWidget* panel, QWidget* seg) {
        if (!panel || !panel->layout() || panel->layout()->count() < 1)
            return;
        QLayoutItem* item = panel->layout()->itemAt(0);
        QWidget* toolbar = item ? item->widget() : nullptr;
        if (auto* lay = toolbar ? qobject_cast<QHBoxLayout*>(toolbar->layout()) : nullptr)
            lay->insertWidget(0, seg);
    };
    insertSeg(m_scriptsPanel, m_segScripts);
    insertSeg(m_eventsPanel, m_segEvents);

    connect(m_segScripts, &oclero::qlementine::SegmentedControl::currentIndexChanged, this, [this]() {
        if (m_segScripts)
            setSegment(m_segScripts->currentIndex());
    });
    connect(m_segEvents, &oclero::qlementine::SegmentedControl::currentIndexChanged, this, [this]() {
        if (m_segEvents)
            setSegment(m_segEvents->currentIndex());
    });

    refreshScripts();

    if (GlobalClient && GlobalClient->extender)
        connect(GlobalClient->extender, &Extender::extensionChanged, this, &ScriptsWidget::refreshScripts);
    connect(adaptixWidget, &AdaptixWidget::serverScriptsChanged, this, &ScriptsWidget::refreshScripts);
    connect(adaptixWidget, &AdaptixWidget::eventHandlersChanged, this, &ScriptsWidget::refreshEventHandlers);

    this->dockWidget->setWidget(this);
}

ScriptsWidget::~ScriptsWidget() = default;

void ScriptsWidget::setSegment(int segment, const QString& originFilter, bool applyOriginFilter)
{
    if (segment < 0)
        segment = 0;
    if (segment > 1)
        segment = 1;

    const bool segmentChanged = (m_currentSegment != segment) || (m_stack && m_stack->currentIndex() != segment);
    m_currentSegment = segment;
    if (m_stack)
        m_stack->setCurrentIndex(segment);
    if (m_segScripts) {
        QSignalBlocker b(m_segScripts);
        m_segScripts->setCurrentIndex(segment);
    }
    if (m_segEvents) {
        QSignalBlocker b(m_segEvents);
        m_segEvents->setCurrentIndex(segment);
    }

    if (segment == 0) {
        if (applyOriginFilter) {
            if (m_scriptsOriginCombo) {
                int comboIdx = 0;
                for (int i = 0; i < m_scriptsOriginCombo->count(); ++i) {
                    if (m_scriptsOriginCombo->itemData(i).toString() == originFilter) {
                        comboIdx = i;
                        break;
                    }
                }
                if (m_scriptsOriginCombo->currentIndex() != comboIdx) {
                    QSignalBlocker b(m_scriptsOriginCombo);
                    m_scriptsOriginCombo->setCurrentIndex(comboIdx);
                }
            }
            if (m_scriptsFilter)
                m_scriptsFilter->setOriginFilter(originFilter);
        }
        if (segmentChanged)
            refreshScripts();
        else if (applyOriginFilter && m_scriptsEmpty) {
            const bool has = m_scriptsFilter && m_scriptsFilter->rowCount() > 0;
            if (auto* stack = qobject_cast<QStackedWidget*>( m_scriptsEmpty->property("stackWidget").value<QObject*>()))
                stack->setCurrentIndex(has ? 0 : 1);
        }
        QTimer::singleShot(0, this, [this]() {
            rebalanceScriptsColumns(m_scriptsTable, ScriptsTableKind::Scripts);
        });
    } else {
        if (segmentChanged)
            loadEventsPage();
        QTimer::singleShot(0, this, [this]() {
            rebalanceScriptsColumns(m_eventsTable, ScriptsTableKind::Events);
        });
    }
}

QStringList ScriptsWidget::selectedIds(QTableView* table, int /*idColumn*/) const
{
    QStringList ids;
    if (!table || !table->selectionModel())
        return ids;
    const QModelIndexList rows = table->selectionModel()->selectedRows();
    for (const QModelIndex& idx : rows) {
        QModelIndex src = idx;
        if (auto* proxy = qobject_cast<QSortFilterProxyModel*>(table->model()))
            src = proxy->mapToSource(idx);
        auto* model = qobject_cast<const QStandardItemModel*>(src.model());
        if (!model)
            continue;
        const QStandardItem* it = model->item(src.row(), 0);
        if (it)
            ids << it->data(kIdRole).toString();
    }
    return ids;
}

QList<ScriptsWidget::ScriptRowSel> ScriptsWidget::selectedScriptRows() const
{
    QList<ScriptRowSel> out;
    if (!m_scriptsTable || !m_scriptsTable->selectionModel())
        return out;
    const QModelIndexList rows = m_scriptsTable->selectionModel()->selectedRows();
    for (const QModelIndex& idx : rows) {
        QModelIndex src = idx;
        if (auto* proxy = qobject_cast<QSortFilterProxyModel*>(m_scriptsTable->model()))
            src = proxy->mapToSource(idx);
        auto* model = qobject_cast<const QStandardItemModel*>(src.model());
        if (!model)
            continue;
        const QStandardItem* it = model->item(src.row(), 0);
        if (!it)
            continue;
        ScriptRowSel row;
        row.id = it->data(kIdRole).toString();
        row.kind = it->data(kKindRole).toString();
        if (!row.id.isEmpty())
            out.append(row);
    }
    return out;
}

void ScriptsWidget::updateEmptyState(QLabel* empty, QAbstractItemModel* model) const
{
    if (!empty)
        return;
    const bool has = model && model->rowCount() > 0;
    if (auto* stack = qobject_cast<QStackedWidget*>(empty->property("stackWidget").value<QObject*>()))
        stack->setCurrentIndex(has ? 0 : 1);
    empty->setVisible(!has);
}

void ScriptsWidget::setupScriptsTable()
{
    m_scriptsPanel = new QWidget(this);
    auto* root = new QVBoxLayout(m_scriptsPanel);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* toolbar = new QWidget(m_scriptsPanel);
    toolbar->setFixedHeight(FontManager::instance().typography().toolbarHeight);
    auto* tb = new QHBoxLayout(toolbar);
    tb->setContentsMargins(10, 0, 10, 0);
    tb->setSpacing(8);

    m_scriptsSearch = new oclero::qlementine::LineEdit(toolbar);
    m_scriptsSearch->setPlaceholderText(QStringLiteral("Search scripts…"));
    styleToolbarSearch(m_scriptsSearch);
    tb->addWidget(m_scriptsSearch, 0);

    m_scriptsOriginCombo = new QComboBox(toolbar);
    m_scriptsOriginCombo->setMinimumWidth(120);
    m_scriptsOriginCombo->setMaximumWidth(180);
    m_scriptsOriginCombo->setFixedHeight(FontManager::instance().typography().controlHeight);
    m_scriptsOriginCombo->addItem(QStringLiteral("All"), QString());
    m_scriptsOriginCombo->addItem(QStringLiteral("Local"), QStringLiteral("local"));
    m_scriptsOriginCombo->addItem(QStringLiteral("Teamserver"), QStringLiteral("server"));
    tb->addWidget(m_scriptsOriginCombo, 0);

    tb->addStretch(1);

    m_scriptsAddBtn = new QPushButton(QStringLiteral("+ Add Script"), toolbar);
    m_scriptsAddBtn->setFixedHeight(FontManager::instance().typography().controlHeight);
    tb->addWidget(m_scriptsAddBtn, 0);
    root->addWidget(toolbar);

    m_scriptsModel = new QStandardItemModel(this);
    m_scriptsModel->setHorizontalHeaderLabels({
        QStringLiteral("Status"), QStringLiteral("Name"),
        QStringLiteral("Location"), QStringLiteral("Description")
    });
    m_scriptsFilter = new ScriptsTableFilter(this);
    m_scriptsFilter->setSourceModel(m_scriptsModel);
    m_scriptsTable = new QTableView(m_scriptsPanel);
    m_scriptsTable->setModel(m_scriptsFilter);
    styleTable(m_scriptsTable);
    applyScriptsColumnLayout(m_scriptsTable, ScriptsTableKind::Scripts);

    m_scriptsEmpty = makeEmptyLabel(m_scriptsPanel, QStringLiteral("No scripts\nUse + Add Script for a local .axs, or wait for teamserver sync"));
    auto* stack = new QStackedWidget(m_scriptsPanel);
    stack->addWidget(m_scriptsTable);
    stack->addWidget(m_scriptsEmpty);
    root->addWidget(stack, 1);

    connect(m_scriptsSearch, &QLineEdit::textChanged, this, [this, stack](const QString& t) {
        if (m_scriptsFilter)
            m_scriptsFilter->setSearchText(t);
        const bool has = m_scriptsFilter && m_scriptsFilter->rowCount() > 0;
        stack->setCurrentIndex(has ? 0 : 1);
    });
    connect(m_scriptsOriginCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, stack]() {
        if (!m_scriptsFilter || !m_scriptsOriginCombo)
            return;
        m_scriptsFilter->setOriginFilter(m_scriptsOriginCombo->currentData().toString());
        const bool has = m_scriptsFilter->rowCount() > 0;
        stack->setCurrentIndex(has ? 0 : 1);
    });
    connect(m_scriptsAddBtn, &QPushButton::clicked, this, &ScriptsWidget::onScriptsLoad);
    connect(m_scriptsTable, &QTableView::customContextMenuRequested, this, &ScriptsWidget::onScriptsMenu);
    connect(m_scriptsTable, &QTableView::doubleClicked, this, &ScriptsWidget::onScriptsDoubleClicked);
    m_scriptsEmpty->setProperty("stackWidget", QVariant::fromValue(static_cast<QObject*>(stack)));
}

void ScriptsWidget::setupEventsTable()
{
    m_eventsPanel = new QWidget(this);
    auto* root = new QVBoxLayout(m_eventsPanel);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* toolbar = new QWidget(m_eventsPanel);
    toolbar->setFixedHeight(FontManager::instance().typography().toolbarHeight);
    auto* tb = new QHBoxLayout(toolbar);
    tb->setContentsMargins(10, 0, 10, 0);
    tb->setSpacing(8);

    m_eventsSearch = new oclero::qlementine::LineEdit(toolbar);
    m_eventsSearch->setPlaceholderText(QStringLiteral("Search handlers…"));
    styleToolbarSearch(m_eventsSearch);
    tb->addWidget(m_eventsSearch, 0);

    m_eventsSourceCombo = new QComboBox(toolbar);
    m_eventsSourceCombo->setMinimumWidth(120);
    m_eventsSourceCombo->setMaximumWidth(180);
    m_eventsSourceCombo->setFixedHeight(FontManager::instance().typography().controlHeight);
    m_eventsSourceCombo->addItem(QStringLiteral("All sources"), QString());
    m_eventsSourceCombo->addItem(QStringLiteral("handler"), QStringLiteral("handler"));
    m_eventsSourceCombo->addItem(QStringLiteral("extender"), QStringLiteral("extender"));
    m_eventsSourceCombo->addItem(QStringLiteral("core"), QStringLiteral("core"));
    tb->addWidget(m_eventsSourceCombo, 0);

    tb->addStretch(1);

    m_eventsPagination = new PaginationBar(toolbar);
    tb->addWidget(m_eventsPagination, 0);

    m_createHandlerBtn = new QPushButton(QStringLiteral("+ Create Handler"), toolbar);
    m_createHandlerBtn->setFixedHeight(FontManager::instance().typography().controlHeight);
    m_createHandlerBtn->setToolTip(QStringLiteral("Open Code Editor with Event Handler profile"));
    m_createHandlerBtn->setCheckable(false);
    tb->addWidget(m_createHandlerBtn, 0);
    root->addWidget(toolbar);

    m_eventsSplit = new QSplitter(Qt::Horizontal, m_eventsPanel);
    m_eventsSplit->setChildrenCollapsible(false);
    m_eventsSplit->setHandleWidth(1);

    auto* typePanel = new QWidget(m_eventsSplit);
    auto* typeLay = new QVBoxLayout(typePanel);
    typeLay->setContentsMargins(0, 0, 0, 0);
    typeLay->setSpacing(0);
    auto* typeHeader = new QLabel(QStringLiteral("Event types"), typePanel);
    typeHeader->setContentsMargins(10, 8, 10, 4);
    const FeedColors fcHdr = FeedColors::fromTheme();
    {
        QPalette pal = typeHeader->palette();
        pal.setColor(QPalette::WindowText, fcHdr.textSecondary);
        typeHeader->setPalette(pal);
        QFont f = typeHeader->font();
        f.setBold(true);
        f.setPointSizeF(qMax(9.0, f.pointSizeF() > 0 ? f.pointSizeF() - 0.5 : 10.0));
        typeHeader->setFont(f);
    }
    typeLay->addWidget(typeHeader);

    m_eventsTypeList = new QListWidget(typePanel);
    m_eventsTypeList->setObjectName(QStringLiteral("eventsTypeList"));
    m_eventsTypeList->setUniformItemSizes(true);
    m_eventsTypeList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_eventsTypeList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_eventsTypeList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_eventsTypeList->setIconSize(QSize(16, 16));
    m_eventsTypeList->setSpacing(1);
    typeLay->addWidget(m_eventsTypeList, 1);
    typePanel->setMinimumWidth(180);
    typePanel->setMaximumWidth(320);

    auto* right = new QWidget(m_eventsSplit);
    auto* rightLay = new QVBoxLayout(right);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(0);

    m_eventsModel = new QStandardItemModel(this);
    m_eventsModel->setHorizontalHeaderLabels({
        QStringLiteral("Status"), QStringLiteral("Event"), QStringLiteral("Handler"),
        QStringLiteral("Source"), QStringLiteral("Group"), QStringLiteral("Description")
    });
    m_eventsTable = new QTableView(right);
    m_eventsTable->setModel(m_eventsModel);
    styleTable(m_eventsTable);
    applyScriptsColumnLayout(m_eventsTable, ScriptsTableKind::Events);

    m_eventsEmpty = makeEmptyLabel(right, QStringLiteral("No event handlers\nUse + Create Handler to open the Event Handler editor"));
    auto* stack = new QStackedWidget(right);
    stack->addWidget(m_eventsTable);
    stack->addWidget(m_eventsEmpty);
    rightLay->addWidget(stack, 1);
    m_eventsEmpty->setProperty("stackWidget", QVariant::fromValue(static_cast<QObject*>(stack)));

    m_eventsSplit->addWidget(typePanel);
    m_eventsSplit->addWidget(right);
    m_eventsSplit->setStretchFactor(0, 0);
    m_eventsSplit->setStretchFactor(1, 1);
    m_eventsSplit->setSizes({220, 800});
    root->addWidget(m_eventsSplit, 1);

    setupEventsPagination();
    rebuildEventTypeList();

    connect(m_eventsSearch, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (m_eventsPageHelper)
            m_eventsPageHelper->setFilterText(text);
    });
    connect(m_eventsSearch, &QLineEdit::editingFinished, this, &ScriptsWidget::onEventsFilterChanged);
    connect(m_eventsSearch, &QLineEdit::returnPressed, this, &ScriptsWidget::onEventsFilterChanged);
    connect(m_eventsSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScriptsWidget::onEventsFilterChanged);
    connect(m_createHandlerBtn, &QPushButton::clicked, this, &ScriptsWidget::onCreateEventHandler);
    connect(m_eventsTable, &QTableView::customContextMenuRequested, this, &ScriptsWidget::onEventsMenu);
    connect(m_eventsTable, &QTableView::doubleClicked, this, &ScriptsWidget::onEventsDoubleClicked);
    connect(m_eventsPagination, &PaginationBar::prevClicked, this, [this]() {
        m_eventsOffset = qMax(0, m_eventsOffset - m_eventsPageSize);
        loadEventsPage();
    });
    connect(m_eventsPagination, &PaginationBar::nextClicked, this, [this]() {
        m_eventsOffset += m_eventsPageSize;
        loadEventsPage();
    });
    connect(m_eventsPagination, &PaginationBar::pageSizeChanged, this, [this](int size) {
        m_eventsPageSize = size;
        if (m_eventsPageHelper)
            m_eventsPageHelper->setPageSize(size);
        m_eventsOffset = 0;
        loadEventsPage();
    });
    connect(m_eventsTypeList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem*, QListWidgetItem*) {
        onEventTypeListCurrentChanged();
    });
    connect(m_eventsTypeList, &QListWidget::customContextMenuRequested, this, &ScriptsWidget::onEventTypeListMenu);
    connect(m_eventsTypeList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        onEventTypeListDoubleClicked();
    });
}

void ScriptsWidget::setupEventsPagination()
{
    AuthProfile* profile = adaptixWidget ? adaptixWidget->GetProfile() : nullptr;
    m_eventsPageHelper = new PagedTableHelper(profile, QStringLiteral("/events/handlers"), this);
    if (m_eventsPagination)
        m_eventsPageSize = m_eventsPagination->pageSize();
    m_eventsPageHelper->setPageSize(m_eventsPageSize);

    connect(m_eventsPageHelper, &PagedTableHelper::pageReady, this, &ScriptsWidget::onEventsPageReady);
    connect(m_eventsPageHelper, &PagedTableHelper::errorOccurred, this, &ScriptsWidget::onEventsPageError);
    connect(m_eventsPageHelper, &PagedTableHelper::loadingChanged, this, [this](bool loading) {
        if (m_eventsTable)
            m_eventsTable->setEnabled(!loading);
        if (m_eventsPagination)
            m_eventsPagination->setLoading(loading);
    });
}

static QStringList knownEventTypes()
{
    return {
        QStringLiteral("agent.new"), QStringLiteral("agent.activate"), QStringLiteral("agent.generate"),
        QStringLiteral("agent.checkin"), QStringLiteral("agent.update"), QStringLiteral("agent.terminate"),
        QStringLiteral("agent.remove"),
        QStringLiteral("listener.start"), QStringLiteral("listener.stop"),
        QStringLiteral("task.create"), QStringLiteral("task.start"), QStringLiteral("task.update_job"),
        QStringLiteral("task.complete"),
        QStringLiteral("credentials.add"), QStringLiteral("credentials.edit"), QStringLiteral("credentials.remove"),
        QStringLiteral("download.start"), QStringLiteral("download.finish"), QStringLiteral("download.remove"),
        QStringLiteral("upload.start"), QStringLiteral("upload.finish"), QStringLiteral("upload.remove"),
        QStringLiteral("screenshot.add"), QStringLiteral("screenshot.remove"),
        QStringLiteral("tunnel.start"), QStringLiteral("tunnel.stop"),
        QStringLiteral("target.add"), QStringLiteral("target.edit"), QStringLiteral("target.remove"),
        QStringLiteral("pivot.create"), QStringLiteral("pivot.remove"),
        QStringLiteral("client.connect"), QStringLiteral("client.disconnect"),
    };
}

void ScriptsWidget::loadEventsPage()
{
    if (!m_eventsPageHelper)
        return;
    if (m_eventsSearch)
        m_eventsPageHelper->setParam(QStringLiteral("q"), m_eventsSearch->text().trimmed());
    if (m_eventsSourceCombo && m_eventsSourceCombo->currentIndex() > 0)
        m_eventsPageHelper->setParam(QStringLiteral("source"), m_eventsSourceCombo->currentData().toString());
    else
        m_eventsPageHelper->setParam(QStringLiteral("source"), QString());
    if (!m_selectedEventType.isEmpty())
        m_eventsPageHelper->setParam(QStringLiteral("event"), m_selectedEventType);
    else
        m_eventsPageHelper->setParam(QStringLiteral("event"), QString());
    m_eventsPageHelper->loadPage(m_eventsOffset);
    loadEventMutes();
}

void ScriptsWidget::loadEventMutes()
{
    if (!adaptixWidget || !adaptixWidget->GetProfile())
        return;
    QPointer<ScriptsWidget> self(this);
    HttpReqEventMutesListAsync(*adaptixWidget->GetProfile(),
        [self](bool success, const QString&, const QJsonObject& resp) {
            if (!self)
                return;
            QSet<QString> muted;
            if (success) {
                QJsonArray items = resp.value(QStringLiteral("items")).toArray();
                for (const QJsonValue& v : items) {
                    if (v.isString())
                        muted.insert(v.toString());
                    else if (v.isObject())
                        muted.insert(v.toObject().value(QStringLiteral("event")).toString());
                }
            }
            self->m_mutedEvents = muted;
            self->rebuildEventTypeList();
        });
}

void ScriptsWidget::rebuildEventTypeList()
{
    if (!m_eventsTypeList)
        return;

    const QString keep = m_selectedEventType;
    QSignalBlocker block(m_eventsTypeList);
    m_eventsTypeList->clear();

    const FeedColors fc = FeedColors::fromTheme();
    const QIcon iconOff = QIcon(QStringLiteral(":/icons/volume_off"));

    auto* allItem = new QListWidgetItem(QStringLiteral("All events"));
    allItem->setData(Qt::UserRole, QString());
    allItem->setToolTip(QStringLiteral("Show handlers for every event type"));
    m_eventsTypeList->addItem(allItem);

    for (const QString& et : knownEventTypes()) {
        const bool muted = m_mutedEvents.contains(et);
        auto* item = muted ? new QListWidgetItem(iconOff, et) : new QListWidgetItem(et);
        item->setData(Qt::UserRole, et);
        item->setData(Qt::UserRole + 1, muted);
        if (muted) {
            item->setForeground(fc.textSecondary);
            item->setBackground(fc.rowDeadBg);
            item->setToolTip(QStringLiteral("%1 — muted\nSelect + context menu / double-click to unmute").arg(et));
        } else {
            item->setForeground(fc.textPrimary);
            item->setToolTip(QStringLiteral("%1 — active\nMulti-select + Mute, or double-click to mute").arg(et));
        }
        m_eventsTypeList->addItem(item);
    }

    int selectRow = 0;
    if (!keep.isEmpty()) {
        for (int i = 0; i < m_eventsTypeList->count(); ++i) {
            if (m_eventsTypeList->item(i)->data(Qt::UserRole).toString() == keep) {
                selectRow = i;
                break;
            }
        }
    }
    m_eventsTypeList->setCurrentRow(selectRow);
}

static QStringList selectedEventTypesFromList(QListWidget* list)
{
    QStringList out;
    if (!list)
        return out;
    const QList<QListWidgetItem*> items = list->selectedItems();
    for (QListWidgetItem* it : items) {
        if (!it)
            continue;
        const QString et = it->data(Qt::UserRole).toString();
        if (!et.isEmpty())
            out.append(et);
    }
    if (out.isEmpty() && list->currentItem()) {
        const QString et = list->currentItem()->data(Qt::UserRole).toString();
        if (!et.isEmpty())
            out.append(et);
    }
    return out;
}

void ScriptsWidget::onEventTypeListCurrentChanged()
{
    if (!m_eventsTypeList)
        return;
    QListWidgetItem* it = m_eventsTypeList->currentItem();
    const QString et = it ? it->data(Qt::UserRole).toString() : QString();
    if (et == m_selectedEventType)
        return;
    m_selectedEventType = et;
    m_eventsOffset = 0;
    loadEventsPage();
}

void ScriptsWidget::onEventTypeListDoubleClicked()
{
    const QStringList types = selectedEventTypesFromList(m_eventsTypeList);
    if (types.isEmpty())
        return;
    bool anyActive = false;
    for (const QString& et : types) {
        if (!m_mutedEvents.contains(et)) {
            anyActive = true;
            break;
        }
    }
    if (anyActive)
        onEventMute(types);
    else
        onEventUnmute(types);
}

void ScriptsWidget::onEventTypeListMenu(const QPoint& pos)
{
    if (!m_eventsTypeList)
        return;
    if (QListWidgetItem* under = m_eventsTypeList->itemAt(pos)) {
        if (!under->isSelected())
            m_eventsTypeList->setCurrentItem(under);
    }

    const QStringList types = selectedEventTypesFromList(m_eventsTypeList);
    if (types.isEmpty())
        return;

    int mutedCount = 0;
    for (const QString& et : types) {
        if (m_mutedEvents.contains(et))
            ++mutedCount;
    }
    const int n = types.size();
    oclero::qlementine::Menu menu;
    auto* muteAct = menu.addAction( QIcon(":/icons/notification_off"), n == 1 ? QStringLiteral("Mute %1").arg(types.first()) : QStringLiteral("Mute %1 event types").arg(n), this, [this, types]() { onEventMute(types); });
    muteAct->setEnabled(mutedCount < n);
    auto* unmuteAct = menu.addAction( QIcon(":/icons/notification"), n == 1 ? QStringLiteral("Unmute %1").arg(types.first()) : QStringLiteral("Unmute %1 event types").arg(n), this, [this, types]() { onEventUnmute(types); });
    unmuteAct->setEnabled(mutedCount > 0);
    menu.exec(m_eventsTypeList->mapToGlobal(pos));
}

void ScriptsWidget::onEventsFilterChanged()
{
    m_eventsOffset = 0;
    loadEventsPage();
}

void ScriptsWidget::updateEventsPageChrome()
{
    const int count = m_eventHandlers.size();
    if (m_eventsPagination) {
        const int from = count > 0 ? m_eventsOffset + 1 : 0;
        const int to = m_eventsOffset + count;
        m_eventsPagination->setInfo(from, to, m_eventsTotal);
        m_eventsPagination->setPrevEnabled(m_eventsOffset > 0);
        m_eventsPagination->setNextEnabled(m_eventsOffset + count < m_eventsTotal);
    }
}

void ScriptsWidget::onEventsPageReady(const QJsonObject& response)
{
    applyEventsPage(response);
}

void ScriptsWidget::onEventsPageError(const QString& message)
{
    Q_UNUSED(message)
    m_eventHandlers.clear();
    if (m_eventsModel)
        m_eventsModel->removeRows(0, m_eventsModel->rowCount());
    m_eventsTotal = 0;
    updateEventsPageChrome();
    updateEmptyState(m_eventsEmpty, m_eventsModel);
}

void ScriptsWidget::applyEventsPage(const QJsonObject& response)
{
    m_eventHandlers.clear();
    if (m_eventsModel)
        m_eventsModel->removeRows(0, m_eventsModel->rowCount());

    const QJsonArray items = response.value(QStringLiteral("items")).toArray();
    for (const QJsonValue& v : items) {
        if (!v.isObject())
            continue;
        EventHandlerInfo info = parseEventHandlerFromJson(v.toObject());
        m_eventHandlers.append(info);

        QString status = info.enabled ? QStringLiteral("Enabled") : QStringLiteral("Disabled");
        if (info.eventMuted)
            status = QStringLiteral("Muted");
        if (!info.lastError.isEmpty())
            status = QStringLiteral("Error");
        const QString title = info.name.isEmpty() ? info.id : info.name;

        QList<QStandardItem*> row;
        row << statusItem(info.id, status, info.source.isEmpty() ? QStringLiteral("handler") : info.source);
        row << textItem(info.event);
        row << textItem(title);
        row << textItem(info.source);
        row << textItem(info.group);
        row << textItem(info.description);
        QString tip = filtersTooltip(info.filters);
        if (!info.lastError.isEmpty())
            tip += QStringLiteral("\nlast_error: %1").arg(info.lastError);
        if (info.lastRunAt > 0)
            tip += QStringLiteral("\nlast_run: %1").arg(
                QDateTime::fromSecsSinceEpoch(info.lastRunAt).toString(QStringLiteral("dd/MM HH:mm:ss")));
        if (!info.description.isEmpty())
            tip += QStringLiteral("\n%1").arg(info.description);
        for (QStandardItem* it : row) {
            if (it)
                it->setToolTip(tip);
        }
        m_eventsModel->appendRow(row);
    }

    m_eventsTotal = response.value(QStringLiteral("total")).toInt();
    updateEventsPageChrome();
    updateEmptyState(m_eventsEmpty, m_eventsModel);
    QTimer::singleShot(0, this, [this]() {
        rebalanceScriptsColumns(m_eventsTable, ScriptsTableKind::Events);
    });
}

void ScriptsWidget::refreshScripts()
{
    if (!m_scriptsModel)
        return;
    m_scriptsModel->removeRows(0, m_scriptsModel->rowCount());

    if (GlobalClient && GlobalClient->extender) {
        for (const auto& ext : GlobalClient->extender->extenderFiles) {
            QString status = QStringLiteral("Disabled");
            if (ext.Enabled)
                status = QStringLiteral("Enabled");
            else if (!ext.Message.isEmpty())
                status = QStringLiteral("Error");
            const QString name = ext.Name.isEmpty() ? QFileInfo(ext.FilePath).fileName() : ext.Name;
            QList<QStandardItem*> row;
            row << statusItem(ext.FilePath, status, QStringLiteral("local"));
            row << textItem(name);
            auto* loc = textItem(ext.FilePath);
            loc->setData(true, kElideMiddleRole);
            row << loc;
            row << textItem(ext.Description);
            if (!ext.Message.isEmpty() && status == QLatin1String("Error"))
                row[0]->setToolTip(ext.Message);
            else
                row[2]->setToolTip(ext.FilePath);
            if (!ext.Description.isEmpty())
                row[3]->setToolTip(ext.Description);
            m_scriptsModel->appendRow(row);
        }
    }

    if (adaptixWidget) {
        for (const auto& info : adaptixWidget->GetServerScripts()) {
            const QString status = info.enabled ? QStringLiteral("Enabled") : QStringLiteral("Disabled");
            const QString location = scriptLocationLabel(info);
            QList<QStandardItem*> row;
            row << statusItem(info.name, status, QStringLiteral("server"));
            row << textItem(info.name);
            auto* loc = textItem(location);
            loc->setData(true, kElideMiddleRole);
            row << loc;
            row << textItem(info.description);
            row[2]->setToolTip(location);
            if (!info.description.isEmpty())
                row[3]->setToolTip(info.description);
            m_scriptsModel->appendRow(row);
        }
    }

    updateEmptyState(m_scriptsEmpty, m_scriptsFilter ? static_cast<QAbstractItemModel*>(m_scriptsFilter) : m_scriptsModel);
    QTimer::singleShot(0, this, [this]() {
        rebalanceScriptsColumns(m_scriptsTable, ScriptsTableKind::Scripts);
    });
}

void ScriptsWidget::refreshEventHandlers()
{
    loadEventsPage(); // also refreshes muted event-type list
}

void ScriptsWidget::onScriptsDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid() || !adaptixWidget)
        return;
    const auto rows = selectedScriptRows();
    if (rows.size() != 1)
        return;
    const auto& r = rows.first();
    if (r.kind == QLatin1String("local") && !r.id.isEmpty()) {
        CodeEditorOpenOptions opts;
        opts.filePath = r.id;
        adaptixWidget->LoadCodeEditorUI(opts);
    }
}

void ScriptsWidget::onScriptsMenu(const QPoint& pos)
{
    oclero::qlementine::Menu menu;
    const auto rows = selectedScriptRows();
    const bool has = !rows.isEmpty();
    const QString nLabel = rows.size() > 1 ? QStringLiteral(" (%1)").arg(rows.size()) : QString();

    bool anyLocal = false, anyServer = false;
    bool anyEnabled = false, anyDisabled = false;
    for (const auto& r : rows) {
        if (r.kind == QLatin1String("local")) {
            anyLocal = true;
            if (GlobalClient && GlobalClient->extender) {
                const auto it = GlobalClient->extender->extenderFiles.constFind(r.id);
                if (it != GlobalClient->extender->extenderFiles.constEnd()) {
                    if (it->Enabled) anyEnabled = true;
                    else anyDisabled = true;
                }
            }
        } else if (r.kind == QLatin1String("server")) {
            anyServer = true;
            if (adaptixWidget) {
                for (const auto& s : adaptixWidget->GetServerScripts()) {
                    if (s.name != r.id)
                        continue;
                    if (s.enabled) anyEnabled = true;
                    else anyDisabled = true;
                    break;
                }
            }
        }
    }

    auto* reloadAction = menu.addAction(QIcon(":/icons/reload"), QStringLiteral("Reload") + nLabel, this, [this, rows]() {
        onScriptsReload(rows);
    });
    reloadAction->setEnabled(has && anyLocal); // only local files on disk

    auto* enableAction = menu.addAction(QIcon(":/icons/check"), QStringLiteral("Enable") + nLabel, this, [this, rows]() {
        onScriptsEnable(rows);
    });
    enableAction->setEnabled(has && anyDisabled);

    auto* disableAction = menu.addAction(QIcon(":/icons/stop"), QStringLiteral("Disable") + nLabel, this, [this, rows]() {
        onScriptsDisable(rows);
    });
    disableAction->setEnabled(has && anyEnabled);

    menu.addSeparator();
    auto* openAction = menu.addAction(QIcon(":/icons/code"), QStringLiteral("Open in Editor"), this, [this, rows]() {
        if (rows.size() != 1 || rows.first().kind != QLatin1String("local") || !adaptixWidget)
            return;
        CodeEditorOpenOptions opts;
        opts.filePath = rows.first().id;
        adaptixWidget->LoadCodeEditorUI(opts);
    });
    openAction->setEnabled(rows.size() == 1 && anyLocal && !anyServer);

    menu.addSeparator();
    auto* removeAction = menu.addAction(QIcon(":/icons/delete"), QStringLiteral("Remove") + nLabel, this, [this, rows]() {
        onScriptsRemove(rows);
    });
    removeAction->setEnabled(has && anyLocal); // only local extensions

    menu.exec(m_scriptsTable->viewport()->mapToGlobal(pos));
}

void ScriptsWidget::onScriptsLoad()
{
    QString baseDir;
    if (GlobalClient && GlobalClient->mainUI) {
        if (auto profile = GlobalClient->mainUI->GetCurrentProfile())
            baseDir = profile->GetProjectDir();
    }
    NonBlockingDialogs::getOpenFileName(this, "Load Script", baseDir, "AxScript Files (*.axs)",
        [this](const QString& filePath) {
            if (filePath.isEmpty() || !GlobalClient || !GlobalClient->extender)
                return;
            GlobalClient->extender->LoadFromFile(filePath, true);
        });
}

void ScriptsWidget::onScriptsReload(const QList<ScriptRowSel>& rows)
{
    if (!GlobalClient || !GlobalClient->extender)
        return;
    for (const auto& r : rows) {
        if (r.kind != QLatin1String("local"))
            continue;
        GlobalClient->extender->RemoveExtension(r.id);
        GlobalClient->extender->LoadFromFile(r.id, true);
    }
}

void ScriptsWidget::onScriptsEnable(const QList<ScriptRowSel>& rows)
{
    for (const auto& r : rows) {
        if (r.kind == QLatin1String("local")) {
            if (GlobalClient && GlobalClient->extender)
                GlobalClient->extender->EnableExtension(r.id);
        } else if (r.kind == QLatin1String("server") && adaptixWidget) {
            adaptixWidget->EnableServerScript(r.id);
        }
    }
    refreshScripts();
}

void ScriptsWidget::onScriptsDisable(const QList<ScriptRowSel>& rows)
{
    for (const auto& r : rows) {
        if (r.kind == QLatin1String("local")) {
            if (GlobalClient && GlobalClient->extender)
                GlobalClient->extender->DisableExtension(r.id);
        } else if (r.kind == QLatin1String("server") && adaptixWidget) {
            adaptixWidget->DisableServerScript(r.id);
        }
    }
    refreshScripts();
}

void ScriptsWidget::onScriptsRemove(const QList<ScriptRowSel>& rows)
{
    if (!GlobalClient || !GlobalClient->extender)
        return;
    for (const auto& r : rows) {
        if (r.kind == QLatin1String("local"))
            GlobalClient->extender->RemoveExtension(r.id);
    }
}

void ScriptsWidget::onEventsMenu(const QPoint& pos)
{
    const QStringList ids = selectedIds(m_eventsTable);
    QString id = ids.isEmpty() ? QString() : ids.first();
    EventHandlerInfo cur;
    for (const auto& h : m_eventHandlers) {
        if (h.id == id) { cur = h; break; }
    }
    const bool has = !id.isEmpty();
    const bool isHandler = cur.isAxHandler();
    oclero::qlementine::Menu menu;
    auto* edit = menu.addAction(QIcon(QStringLiteral(":/icons/code")), QStringLiteral("Edit…"), this, [this, id]() {
        onEditEventHandler(id);
    });
    edit->setEnabled(has && isHandler);
    menu.addSeparator();
    auto* en = menu.addAction(QIcon(":/icons/check"), "Enable", this, [this, id]() { onEventEnable({id}); });
    en->setEnabled(has && !cur.enabled);
    auto* dis = menu.addAction(QIcon(":/icons/close"), "Disable", this, [this, id]() { onEventDisable({id}); });
    dis->setEnabled(has && cur.enabled);
    menu.addSeparator();
    auto* mute = menu.addAction(QIcon(":/icons/notification_off"), "Mute event type", this, [this, ev = cur.event]() {
        if (!ev.isEmpty()) onEventMute({ev});
    });
    mute->setEnabled(has && !cur.event.isEmpty());
    auto* unmute = menu.addAction(QIcon(":/icons/notification"), "Unmute event type", this, [this, ev = cur.event]() {
        if (!ev.isEmpty()) onEventUnmute({ev});
    });
    unmute->setEnabled(has && !cur.event.isEmpty());
    menu.addSeparator();
    auto* rem = menu.addAction(QIcon(":/icons/delete"), "Remove", this, [this, id]() { onEventRemove({id}); });
    rem->setEnabled(has && isHandler);
    menu.exec(m_eventsTable->viewport()->mapToGlobal(pos));
}

void ScriptsWidget::onEventsDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid() || !m_eventsTable)
        return;

    m_eventsTable->selectRow(index.row());

    QString id;
    QModelIndex col0 = index.sibling(index.row(), 0);
    if (auto* proxy = qobject_cast<QSortFilterProxyModel*>(m_eventsTable->model()))
        col0 = proxy->mapToSource(col0);
    if (col0.isValid())
        id = col0.data(kIdRole).toString();
    if (id.isEmpty()) {
        const QStringList ids = selectedIds(m_eventsTable);
        id = ids.isEmpty() ? QString() : ids.first();
    }
    if (id.isEmpty())
        return;

    for (const auto& h : m_eventHandlers) {
        if (h.id != id)
            continue;
        if (h.isAxHandler())
            onEditEventHandler(id);
        return;
    }
}

void ScriptsWidget::onEditEventHandler(const QString& id)
{
    if (id.isEmpty() || !adaptixWidget || !adaptixWidget->GetProfile())
        return;
    QPointer<ScriptsWidget> self(this);
    HttpReqEventHandlerGetAsync(id, *adaptixWidget->GetProfile(),
        [self](bool success, const QString& message, const QJsonObject& resp) {
            if (!self || !self->adaptixWidget)
                return;
            if (!success) {
                QMessageBox::warning(self, QStringLiteral("Open handler"), message);
                return;
            }
            QJsonObject data = resp;
            if (resp.contains(QStringLiteral("data")) && resp.value(QStringLiteral("data")).isObject())
                data = resp.value(QStringLiteral("data")).toObject();
            const QString script = data.value(QStringLiteral("script")).toString();
            const QString name = data.value(QStringLiteral("name")).toString();
            const QString event = data.value(QStringLiteral("event")).toString();
            const QString group = data.value(QStringLiteral("group")).toString();
            const QString desc = data.value(QStringLiteral("description")).toString();
            const QString hid = data.value(QStringLiteral("id")).toString();

            CodeEditorOpenOptions opts;
            opts.profile = QStringLiteral("system.event_handler");
            opts.restrictProfiles = true;
            opts.profiles = QStringList{ QStringLiteral("system.event_handler") };
            opts.contentName = (name.isEmpty() ? hid : name) + QStringLiteral(".axs");
            if (!hid.isEmpty())
                opts.documentKey = QStringLiteral("event-handler:") + hid;
            opts.content = script;
            opts.panelSeed.insert(QStringLiteral("id"), hid);
            opts.panelSeed.insert(QStringLiteral("event"), event);
            opts.panelSeed.insert(QStringLiteral("name"), name);
            opts.panelSeed.insert(QStringLiteral("description"), desc);
            opts.panelSeed.insert(QStringLiteral("group"), group.isEmpty() ? name : group);
            if (data.value(QStringLiteral("filters")).isObject()) {
                const QJsonObject f = data.value(QStringLiteral("filters")).toObject();
                auto firstId = [&](const char* key, const QString& panelKey) {
                    if (!f.value(QString::fromUtf8(key)).isArray())
                        return;
                    const QJsonArray arr = f.value(QString::fromUtf8(key)).toArray();
                    if (!arr.isEmpty())
                        opts.panelSeed.insert(panelKey, arr.at(0).toVariant().toString());
                };
                auto firstStr = [&](const char* key, const QString& panelKey) {
                    if (!f.value(QString::fromUtf8(key)).isArray())
                        return;
                    const QJsonArray arr = f.value(QString::fromUtf8(key)).toArray();
                    if (!arr.isEmpty())
                        opts.panelSeed.insert(panelKey, arr.at(0).toString());
                };
                firstId("agent_ids", QStringLiteral("agent_id"));
                firstId("task_ids", QStringLiteral("task_id"));
                firstId("file_ids", QStringLiteral("file_id"));
                firstId("ports", QStringLiteral("port"));
                firstStr("agent_names", QStringLiteral("agent_name"));
                firstStr("users", QStringLiteral("user"));
                firstStr("os", QStringLiteral("os"));
                firstStr("computers", QStringLiteral("computer"));
                firstStr("tags", QStringLiteral("tags"));
                firstStr("listeners", QStringLiteral("listener"));
                firstStr("listener_types", QStringLiteral("listener_type"));
                firstStr("listener_tags", QStringLiteral("listener_tag"));
                firstStr("clients", QStringLiteral("client"));
                firstStr("filenames", QStringLiteral("filename"));
                firstStr("tunnel_types", QStringLiteral("tunnel_type"));
                firstStr("realms", QStringLiteral("realm"));
                firstStr("cred_types", QStringLiteral("cred_type"));
                firstStr("hosts", QStringLiteral("host"));
                firstStr("domains", QStringLiteral("domain"));
                firstStr("addresses", QStringLiteral("address"));
                if (f.contains(QStringLiteral("alive")) && f.value(QStringLiteral("alive")).isBool())
                    opts.panelSeed.insert(QStringLiteral("alive"),
                        f.value(QStringLiteral("alive")).toBool() ? QStringLiteral("true") : QStringLiteral("false"));
            }
            self->adaptixWidget->LoadCodeEditorUI(opts);
        });
}

void ScriptsWidget::runEventHttpBatch(const QStringList& keys, const QString& failTitle, EventHttpOp op)
{
    if (!adaptixWidget || !adaptixWidget->GetProfile() || keys.isEmpty())
        return;
    AuthProfile* profile = adaptixWidget->GetProfile();
    QPointer<ScriptsWidget> guard(this);
    const auto cb = [guard, failTitle](bool success, const QString& message, const QJsonObject&) {
        if (!guard)
            return;
        if (!success) {
            QMessageBox::warning(guard, failTitle, message);
            return;
        }
        guard->refreshEventHandlers();
    };
    for (const QString& key : keys) {
        switch (op) {
        case EventHttpOp::Enable:  HttpReqEventHandlerEnableAsync(key, *profile, cb); break;
        case EventHttpOp::Disable: HttpReqEventHandlerDisableAsync(key, *profile, cb); break;
        case EventHttpOp::Remove:  HttpReqEventHandlerRemoveAsync(key, *profile, cb); break;
        case EventHttpOp::Mute:    HttpReqEventMuteAsync(key, *profile, cb); break;
        case EventHttpOp::Unmute:  HttpReqEventUnmuteAsync(key, *profile, cb); break;
        }
    }
}

void ScriptsWidget::onEventEnable(const QStringList& ids)
{
    runEventHttpBatch(ids, QStringLiteral("Enable failed"), EventHttpOp::Enable);
}

void ScriptsWidget::onEventDisable(const QStringList& ids)
{
    runEventHttpBatch(ids, QStringLiteral("Disable failed"), EventHttpOp::Disable);
}

void ScriptsWidget::onEventRemove(const QStringList& ids)
{
    runEventHttpBatch(ids, QStringLiteral("Remove failed"), EventHttpOp::Remove);
}

void ScriptsWidget::onEventMute(const QStringList& events)
{
    runEventHttpBatch(events, QStringLiteral("Mute failed"), EventHttpOp::Mute);
    for (const QString& e : events)
        m_mutedEvents.insert(e);
    rebuildEventTypeList();
}

void ScriptsWidget::onEventUnmute(const QStringList& events)
{
    runEventHttpBatch(events, QStringLiteral("Unmute failed"), EventHttpOp::Unmute);
    for (const QString& e : events)
        m_mutedEvents.remove(e);
    rebuildEventTypeList();
}

static QString loadAxTemplate(const QString& relativePath)
{
    static QHash<QString, QString> cache;
    const auto it = cache.constFind(relativePath);
    if (it != cache.constEnd())
        return it.value();
    QFile f(QStringLiteral(":/axscript/") + relativePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QString text = QString::fromUtf8(f.readAll());
    cache.insert(relativePath, text);
    return text;
}

static QString defaultHandlerBodyHint(const QString& eventType, const QString& hName)
{
    QString bodyPath = QStringLiteral("templates/handler_body_generic.axs");
    if (eventType == QLatin1String("agent.new") || eventType == QLatin1String("agent.activate"))
        bodyPath = QStringLiteral("templates/handler_body_agent.axs");
    else if (eventType.startsWith(QLatin1String("task.")))
        bodyPath = QStringLiteral("templates/handler_body_task.axs");
    else if (eventType.startsWith(QLatin1String("client.")))
        bodyPath = QStringLiteral("templates/handler_body_client.axs");

    QString body = loadAxTemplate(bodyPath);
    if (body.isEmpty()) {
        body = QStringLiteral("    ax.log(\"%1: type=\" + event.type);").arg(hName);
    } else {
        body.replace(QStringLiteral("{{NAME}}"), hName);
        if (body.endsWith(QLatin1Char('\n')))
            body.chop(1);
    }

    QString shell = loadAxTemplate(QStringLiteral("templates/handler_shell.axs"));
    if (shell.isEmpty()) {
        return QStringLiteral(
            "// Event handler — must define function handler(event)\n"
            "function handler(event) {\n"
            "%1\n"
            "}\n"
        ).arg(body);
    }
    return shell.replace(QStringLiteral("{{BODY}}"), body);
}

QString ScriptsWidget::nextDefaultHandlerName() const
{
    QString base = QStringLiteral("auto_handler");
    QSet<QString> used;
    for (const auto& h : m_eventHandlers) {
        if (!h.name.isEmpty())
            used.insert(h.name);
    }
    if (m_eventsModel) {
        for (int r = 0; r < m_eventsModel->rowCount(); ++r) {
            if (auto* it = m_eventsModel->item(r, 2))
                used.insert(it->text());
        }
    }
    if (!used.contains(base))
        return base;
    for (int i = 2; i < 1000; ++i) {
        const QString cand = QStringLiteral("%1_%2").arg(base).arg(i);
        if (!used.contains(cand))
            return cand;
    }
    return base + QStringLiteral("_new");
}

void ScriptsWidget::onCreateEventHandler()
{
    if (!adaptixWidget)
        return;

    const QString eventType = QStringLiteral("agent.new");
    const QString scriptName = nextDefaultHandlerName();
    const QString handlerName = scriptName;
    const QString description = QStringLiteral("Handler for %1").arg(eventType);
    const QString body = defaultHandlerBodyHint(eventType, handlerName);
    const QString fileName = scriptName + QStringLiteral(".axs");

    CodeEditorOpenOptions opts;
    opts.profile = QStringLiteral("system.event_handler");
    opts.restrictProfiles = true;
    opts.profiles = QStringList{ QStringLiteral("system.event_handler") };
    opts.contentName = fileName;
    opts.content = body;
    opts.panelSeed.insert(QStringLiteral("event"), eventType);
    opts.panelSeed.insert(QStringLiteral("name"), handlerName);
    opts.panelSeed.insert(QStringLiteral("description"), description);
    opts.panelSeed.insert(QStringLiteral("group"), scriptName);
    adaptixWidget->LoadCodeEditorUI(opts);
}
