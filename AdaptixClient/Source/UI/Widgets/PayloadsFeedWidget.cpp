#include <UI/Widgets/PayloadsFeedWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Dialogs/DialogAgent.h>
#include <UI/Dialogs/DialogPayload.h>
#include <Client/AuthProfile.h>
#include <Client/Requestor.h>
#include <Client/Settings.h>
#include <Client/PagedTableHelper.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <Utils/NonBlockingDialogs.h>
#include <Utils/FontManager.h>
#include <Utils/Logs.h>
#include <Utils/CustomElements/BoldHeaderView.h>
#include <Utils/CustomElements/ListFeed.h>
#include <MainAdaptix.h>

#include <QJSEngine>
#include <QJSValue>

#include <oclero/qlementine/widgets/Menu.hpp>
#include <oclero/qlementine/widgets/LineEdit.hpp>

#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QFile>
#include <QDialog>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QDir>
#include <QTableView>
#include <QLabel>
#include <QStackedWidget>
#include <QToolButton>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QToolTip>
#include <QHelpEvent>
#include <QAbstractItemView>
#include <QStandardItem>
#include <QTimer>
#include <QColorDialog>
#include <QColor>
#include <QInputDialog>

static constexpr int kPayloadIdRole   = Qt::UserRole + 1;
static constexpr int kHiddenRole      = Qt::UserRole + 2;
static constexpr int kElideMiddleRole = Qt::UserRole + 3;
static constexpr int kFullHashRole    = Qt::UserRole + 4;

class PayloadsTableDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const FeedColors fc = FeedColors::fromTheme();
        const bool selected = opt.state & QStyle::State_Selected;
        const bool hidden = index.data(kHiddenRole).toBool() || index.sibling(index.row(), 0).data(kHiddenRole).toBool();
        auto roleColor = [](const QModelIndex& idx, int role) -> QColor {
            const QVariant v = idx.data(role);
            if (!v.isValid())
                return {};
            if (v.canConvert<QBrush>())
                return v.value<QBrush>().color();
            if (v.canConvert<QColor>())
                return v.value<QColor>();
            return {};
        };

        QColor customBg;
        if (hidden) {
            customBg = fc.rowDeadBg;
        } else {
            customBg = roleColor(index, Qt::BackgroundRole);
        }
        paintFeedTableCellBackground(painter, opt.rect, selected, /*hovered=*/false, index.row() % 2 == 1, index.column() == 0, customBg);

        const bool isHash = index.data(kFullHashRole).isValid();
        QRect textRect = opt.rect.adjusted(index.column() == 0 ? 12 : 8, 0, -8, 0);
        QColor pen = opt.palette.color(QPalette::Text);
        if (hidden) {
            pen = fc.dark ? fc.textDead.lighter(120) : fc.textDead.darker(120);
        } else {
            const QColor fg = roleColor(index, Qt::ForegroundRole);
            if (fg.isValid())
                pen = fg;
        }
        painter->setPen(pen);
        painter->setFont(opt.font);

        QString text = index.data(kFullHashRole).toString();
        if (text.isEmpty())
            text = opt.text;
        const bool mid = index.data(kElideMiddleRole).toBool() || isHash;
        const QString elided = opt.fontMetrics.elidedText( text, mid ? Qt::ElideMiddle : Qt::ElideRight, textRect.width());
        const int align = isHash ? (Qt::AlignHCenter | Qt::AlignVCenter) : (Qt::AlignLeft | Qt::AlignVCenter);
        painter->drawText(textRect, align, elided);
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        const QFontMetrics fm(opt.font);
        const int hPad = index.column() == 0 ? 20 : 16;
        const bool isHash = index.data(kFullHashRole).isValid();
        const int textW = isHash ? 80 : fm.horizontalAdvance(opt.text);
        const int w = qMax(48, textW + hPad + 4);
        const int h = qMax(fm.height() + 10, 30);
        return QSize(w, h);
    }

    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option, const QModelIndex& index) override
    {
        if (event && event->type() == QEvent::ToolTip) {
            QString tip = index.data(kFullHashRole).toString();
            if (tip.isEmpty())
                tip = index.data(Qt::ToolTipRole).toString();
            if (!tip.isEmpty()) {
                QToolTip::showText(event->globalPos(), tip, view);
                return true;
            }
        }
        return QStyledItemDelegate::helpEvent(event, view, option, index);
    }
};

static QStandardItem* textItem(const QString& t, bool bold = false, bool muted = false, bool elideMiddle = false)
{
    auto* it = new QStandardItem(t);
    it->setEditable(false);
    if (!t.isEmpty())
        it->setToolTip(t);
    if (bold) {
        QFont f = it->font();
        f.setBold(true);
        it->setFont(f);
    }
    const FeedColors fc = FeedColors::fromTheme();
    if (muted)
        it->setForeground(fc.textSecondary);
    else if (bold)
        it->setForeground(fc.textPrimary);
    if (elideMiddle)
        it->setData(true, kElideMiddleRole);
    return it;
}

static void fitPayloadColumns(QTableView* tv, int firstHashCol, int lastHashCol)
{
    if (!tv || !tv->model() || !tv->horizontalHeader())
        return;
    auto* h = tv->horizontalHeader();
    const int n = tv->model()->columnCount();
    if (n <= 0)
        return;

    h->setStretchLastSection(false);
    h->setMinimumSectionSize(40);

    for (int c = 0; c < n; ++c)
        h->setSectionResizeMode(c, QHeaderView::ResizeToContents);

    tv->resizeColumnsToContents();

    for (int c = 0; c < n; ++c) {
        if (c >= firstHashCol && c <= lastHashCol) {
            h->setSectionResizeMode(c, QHeaderView::Stretch);
            h->setMinimumSectionSize(72);
            continue;
        }
        const QString label = tv->model()->headerData(c, Qt::Horizontal).toString();
        const int headerW = h->fontMetrics().horizontalAdvance(label) + 28;
        const int contentW = h->sectionSize(c);
        const int w = qMax(contentW, headerW);
        h->setSectionResizeMode(c, QHeaderView::Interactive);
        h->resizeSection(c, w);
    }
}

static void stylePayloadTable(QTableView* tv)
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
    tv->setItemDelegate(new PayloadsTableDelegate(tv));
    applyFeedTableViewChrome(tv);
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

PayloadsFeedWidget::PayloadsFeedWidget(AdaptixWidget* w) : QWidget(w), m_adaptixWidget(w)
{
    applyFeedWidgetSurface(this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    toolbarWidget = new QWidget(this);
    toolbarWidget->setFixedHeight(FontManager::instance().typography().toolbarHeight);
    toolbarWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(0);

    auto* searchWidget = new QWidget(toolbarWidget);
    auto* searchLay = new QHBoxLayout(searchWidget);
    searchLay->setContentsMargins(0, 3, 0, 3);
    searchLay->setSpacing(4);

    auto* search = new oclero::qlementine::LineEdit(searchWidget);
    search->setIcon(QIcon(QStringLiteral(":/icons/search")));
    search->setPlaceholderText(QStringLiteral("Search payloads…"));
    search->setClearButtonEnabled(true);
    search->setMinimumWidth(180);
    search->setMaximumWidth(320);
    search->setFixedHeight(FontManager::instance().typography().controlHeight);
    searchEdit = search;
    searchLay->addWidget(searchEdit);

    showHiddenButton = new QToolButton(searchWidget);
    showHiddenButton->setCheckable(true);
    showHiddenButton->setAutoRaise(true);
    showHiddenButton->setCursor(Qt::PointingHandCursor);
    showHiddenButton->setFocusPolicy(Qt::NoFocus);
    showHiddenButton->setToolTip(QStringLiteral("Show hidden entries"));
    {
        const int h = FontManager::instance().typography().controlHeight;
        showHiddenButton->setFixedSize(h, h);
        showHiddenButton->setIconSize(QSize(qMax(14, h - 10), qMax(14, h - 10)));
    }
    showHiddenButton->setChecked(false);
    auto updateShowHiddenIcon = [this]() {
        showHiddenButton->setIcon(QIcon(showHiddenButton->isChecked() ? QStringLiteral(":/icons/visibility") : QStringLiteral(":/icons/visibility_off")));
        showHiddenButton->setToolTip(showHiddenButton->isChecked() ? QStringLiteral("Showing hidden entries") : QStringLiteral("Show hidden entries"));
    };
    updateShowHiddenIcon();
    connect(showHiddenButton, &QToolButton::toggled, this, [this, updateShowHiddenIcon](bool checked) {
        updateShowHiddenIcon();
        onShowHiddenToggled(checked);
    });
    searchLay->addWidget(showHiddenButton);
    searchLay->addStretch();
    toolbarLayout->addWidget(searchWidget);

    toolbarLayout->addStretch();

    pagination = new PaginationBar(toolbarWidget);
    toolbarLayout->addWidget(pagination);

    auto addToolbarWidgetAfter = [this](QWidget* widget) {
        auto* spacer1 = new QWidget(toolbarWidget);
        spacer1->setFixedWidth(8);
        auto* sep = new QWidget(toolbarWidget);
        sep->setFixedWidth(1);
        sep->setFixedHeight(20);
        sep->setStyleSheet(QStringLiteral("background: rgba(255,255,255,0.08);"));
        auto* spacer2 = new QWidget(toolbarWidget);
        spacer2->setFixedWidth(8);
        toolbarLayout->addWidget(spacer1);
        toolbarLayout->addWidget(sep);
        toolbarLayout->addWidget(spacer2);
        toolbarLayout->addWidget(widget);
    };

    uploadButton = new QPushButton(toolbarWidget);
    uploadButton->setIcon(QIcon(QStringLiteral(":/icons/file_open")));
    uploadButton->setToolTip(QStringLiteral("Import from file"));
    uploadButton->setFixedSize(28, 28);
    addToolbarWidgetAfter(uploadButton);

    generateButton = new QPushButton(QStringLiteral("+ Generate"), toolbarWidget);
    generateButton->setToolTip(QStringLiteral("Generate agent payload"));
    addToolbarWidgetAfter(generateButton);

    root->addWidget(toolbarWidget);

    model = new QStandardItemModel(this);
    model->setHorizontalHeaderLabels({
        QStringLiteral("ID"), QStringLiteral("Name"), QStringLiteral("Description"),
        QStringLiteral("Type"), QStringLiteral("Artifact"), QStringLiteral("Listener(s)"),
        QStringLiteral("Size"), QStringLiteral("Creator"), QStringLiteral("Created"),
        QStringLiteral("UID"), QStringLiteral("Tag"),
        QStringLiteral("MD5"), QStringLiteral("SHA1"), QStringLiteral("SHA256")
    });

    table = new QTableView(this);
    table->setModel(model);
    stylePayloadTable(table);
    UpdateColumnsVisible();
    fitColumns();

    emptyLabel = makeEmptyLabel(this, QStringLiteral("No payloads\nUse + Generate or import a file"));
    contentStack = new QStackedWidget(this);
    contentStack->addWidget(table);
    contentStack->addWidget(emptyLabel);
    root->addWidget(contentStack, 1);

    connect(searchEdit, &QLineEdit::textChanged, this, &PayloadsFeedWidget::onSearchChanged);
    connect(generateButton, &QPushButton::clicked, this, &PayloadsFeedWidget::actionGenerate);
    connect(uploadButton, &QPushButton::clicked, this, &PayloadsFeedWidget::actionImport);
    connect(table, &QTableView::customContextMenuRequested, this, &PayloadsFeedWidget::handleContextMenu);
    connect(table, &QTableView::doubleClicked, this, &PayloadsFeedWidget::onRowDoubleClicked);

    dockWidget = new KDDockWidgets::QtWidgets::DockWidget("PayloadsFeed:Dock-" + w->GetProfile()->GetProject(), KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    dockWidget->setTitle(QStringLiteral("Payload Store"));
    dockWidget->setWidget(this);
    dockWidget->setIcon(QIcon(QStringLiteral(":/icons/kill")), KDDockWidgets::IconPlace::TabBar);

    setupPagination();
    updateEmptyState();
    QTimer::singleShot(0, this, [this]() { loadCurrentPage(); });
}

PayloadsFeedWidget::~PayloadsFeedWidget() = default;

KDDockWidgets::QtWidgets::DockWidget* PayloadsFeedWidget::dock() { return dockWidget; }

void PayloadsFeedWidget::setupPagination()
{
    if (!m_adaptixWidget || !m_adaptixWidget->GetProfile() || !pagination)
        return;

    pageHelper = new PagedTableHelper(m_adaptixWidget->GetProfile(), QStringLiteral("/payload/list"), this);
    pageHelper->setPageSize(pagination->pageSize());

    connect(pageHelper, &PagedTableHelper::pageReady, this, &PayloadsFeedWidget::onPageReady);
    connect(pageHelper, &PagedTableHelper::errorOccurred, this, &PayloadsFeedWidget::onPageError);
    connect(pageHelper, &PagedTableHelper::loadingChanged, this, [this](bool loading) {
        if (pagination)
            pagination->setLoading(loading);
        if (table)
            table->setEnabled(!loading);
    });
    connect(pagination, &PaginationBar::prevClicked, this, [this]() {
        m_offset = qMax(0, m_offset - pagination->pageSize());
        loadCurrentPage();
    });
    connect(pagination, &PaginationBar::nextClicked, this, [this]() {
        m_offset += pagination->pageSize();
        loadCurrentPage();
    });
    connect(pagination, &PaginationBar::pageSizeChanged, this, [this](int size) {
        if (pageHelper)
            pageHelper->setPageSize(size);
        m_offset = 0;
        loadCurrentPage();
    });
}

void PayloadsFeedWidget::loadCurrentPage()
{
    if (!pageHelper)
        return;
    pageHelper->setParam(QStringLiteral("show_hidden"), showHidden() ? QStringLiteral("1") : QStringLiteral("0"));
    pageHelper->setParam(QStringLiteral("sort"), m_sortCol);
    pageHelper->setParam(QStringLiteral("order"), m_sortOrder);
    if (searchEdit)
        pageHelper->setParam(QStringLiteral("q"), searchEdit->text().trimmed());
    pageHelper->loadPage(m_offset);
}

void PayloadsFeedWidget::SetUpdatesEnabled(bool enabled)
{
    setUpdatesEnabled(enabled);
    if (table)
        table->setUpdatesEnabled(enabled);
    if (enabled && !m_cachePrimed)
        loadCurrentPage();
}

void PayloadsFeedWidget::Clear()
{
    m_items.clear();
    m_offset = 0;
    m_cachePrimed = false;
    if (model)
        model->removeRows(0, model->rowCount());
    updatePaginationChrome(0, 0);
    updateEmptyState();
}

bool PayloadsFeedWidget::showHidden() const
{
    return showHiddenButton && showHiddenButton->isChecked();
}

QString PayloadsFeedWidget::formatSize(qint64 bytes)
{
    if (bytes < 1024)
        return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024)
        return QString("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 1);
}

QString PayloadsFeedWidget::formatArtifact(const QString& artifact, const QString& arch)
{
    const QString art = artifact.trimmed().isEmpty() ? QStringLiteral("bin") : artifact.trimmed();
    const QString a = arch.trimmed().toLower();
    if (a.isEmpty() || a == QStringLiteral("unknown") || a == QStringLiteral("any"))
        return art;

    QString archLabel;
    if (a == QStringLiteral("x64") || a == QStringLiteral("amd64") || a == QStringLiteral("x86_64"))
        archLabel = QStringLiteral("intel, x64");
    else if (a == QStringLiteral("x86") || a == QStringLiteral("i386") || a == QStringLiteral("386"))
        archLabel = QStringLiteral("intel, x86");
    else if (a == QStringLiteral("arm64") || a == QStringLiteral("aarch64"))
        archLabel = QStringLiteral("arm, arm64");
    else if (a == QStringLiteral("arm"))
        archLabel = QStringLiteral("arm");
    else
        archLabel = arch.trimmed();

    return QStringLiteral("%1 (%2)").arg(art, archLabel);
}

QString PayloadsFeedWidget::truncHash(const QString& h, int n)
{
    if (h.size() <= n)
        return h;
    return h.left(n) + QString::fromUtf8("…");
}

PayloadData PayloadsFeedWidget::parsePayloadObject(const QJsonObject& o)
{
    PayloadData p;
    p.PayloadId = static_cast<qint64>(o.value("p_id").toDouble());
    p.Name = o.value("p_name").toString();
    p.AgentType = o.value("p_type").toString();
    p.Artifact = o.value("p_artifact").toString();
    p.Arch = o.value("p_arch").toString();
    if (o.value("p_listeners").isArray()) {
        for (const QJsonValue& lv : o.value("p_listeners").toArray())
            p.Listeners << lv.toString();
    }
    p.Size = static_cast<qint64>(o.value("p_size").toDouble());
    p.Sha1 = o.value("p_sha1").toString();
    p.Sha256 = o.value("p_sha256").toString();
    p.Md5 = o.value("p_md5").toString();
    p.Creator = o.value("p_creator").toString();
    p.Created = static_cast<qint64>(o.value("p_date").toDouble());
    p.Hidden = o.value("p_hidden").toBool();
    p.Filename = o.value("p_filename").toString();
    p.BuildId = o.value("p_build_id").toString();
    p.Watermark = o.value("p_watermark").toString();
    p.ConfigJson = o.value("p_config").toString();
    p.Description = o.value("p_notes").toString();
    p.Tag = o.value("p_tag").toString();
    p.Uid = o.value("p_uid").toString();
    p.Color = o.value("p_color").toString();
    p.Missing = o.value("p_missing").toBool();
    return p;
}

QList<QStandardItem*> PayloadsFeedWidget::makeRow(const PayloadData& p)
{
    const bool dim = p.Hidden || p.Missing;

    QColor bgColor;
    QColor fgColor;
    if (!p.Color.isEmpty() && !dim) {
        const QStringList colors = p.Color.split(QLatin1Char('-'));
        if (!colors.isEmpty() && !colors[0].isEmpty())
            bgColor = QColor(colors[0]);
        if (colors.size() > 1 && !colors[1].isEmpty())
            fgColor = QColor(colors[1]);
    }

    auto decorate = [&](QStandardItem* it) {
        it->setData(p.PayloadId, kPayloadIdRole);
        it->setData(p.Hidden || p.Missing, kHiddenRole);
        if (bgColor.isValid())
            it->setBackground(bgColor);
        if (fgColor.isValid())
            it->setForeground(fgColor);
        return it;
    };

    auto makeCell = [&](const QString& t, bool bold = false, bool muted = false) {
        return decorate(textItem(t, bold && !dim, muted && !dim, false));
    };

    auto* idIt = makeCell(QString("#%1").arg(p.PayloadId), false, true);
    idIt->setData(static_cast<qint64>(p.PayloadId), Qt::UserRole);

    QString name = p.Name;
    if (p.Missing)
        name += QStringLiteral(" [missing]");
    auto* nameIt = makeCell(name, !dim, false);

    auto* descIt = makeCell(p.Description);
    if (!p.Description.isEmpty())
        descIt->setToolTip(p.Description);

    auto* typeIt = makeCell(p.AgentType);
    auto* artIt = makeCell(formatArtifact(p.Artifact, p.Arch));
    auto* lisIt = makeCell(p.Listeners.join(QStringLiteral(", ")));

    auto* sizeIt = makeCell(formatSize(p.Size));
    sizeIt->setData(p.Size, Qt::UserRole);

    auto* creatorIt = makeCell(p.Creator);
    const QString created = p.Created > 0 ? QDateTime::fromSecsSinceEpoch(p.Created).toString("yyyy-MM-dd HH:mm:ss") : QString();
    auto* createdIt = makeCell(created);
    createdIt->setData(p.Created, Qt::UserRole);

    auto* uidIt = makeCell(p.Uid);

    auto makeHash = [&](const QString& full) {
        auto* it = decorate(textItem(full, false, false, true));
        it->setToolTip(full);
        it->setData(full, kFullHashRole);
        it->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        return it;
    };

    auto* tagIt = makeCell(p.Tag);
    auto* md5It = makeHash(p.Md5);
    auto* sha1It = makeHash(p.Sha1);
    auto* sha256It = makeHash(p.Sha256);

    return { idIt, nameIt, descIt, typeIt, artIt, lisIt, sizeIt, creatorIt, createdIt, uidIt, tagIt, md5It, sha1It, sha256It };
}

void PayloadsFeedWidget::applyPage(const QJsonObject& response)
{
    if (!model)
        return;

    const bool sorting = table && table->isSortingEnabled();
    if (table)
        table->setSortingEnabled(false);

    model->removeRows(0, model->rowCount());
    m_items.clear();

    const QJsonArray items = response.value(QStringLiteral("items")).toArray();
    for (const QJsonValue& v : items) {
        if (!v.isObject())
            continue;
        PayloadData p = parsePayloadObject(v.toObject());
        if (p.PayloadId <= 0)
            continue;
        m_items.insert(p.PayloadId, p);
        model->appendRow(makeRow(p));
    }

    if (table)
        table->setSortingEnabled(sorting);

    const int total = response.value(QStringLiteral("total")).toInt();
    const int shown = m_items.size();
    updatePaginationChrome(shown, total);
    UpdateColumnsVisible();
    m_cachePrimed = true;
    updateEmptyState();
}

void PayloadsFeedWidget::updatePaginationChrome(int shown, int total)
{
    if (!pagination)
        return;
    const int from = shown == 0 ? 0 : m_offset + 1;
    const int to = m_offset + shown;
    pagination->setInfo(from, to, total);
    pagination->setPrevEnabled(m_offset > 0);
    pagination->setNextEnabled(m_offset + shown < total);
}

void PayloadsFeedWidget::fitColumns()
{
    if (table)
        fitPayloadColumns(table, ColMd5, ColSha256);
}

void PayloadsFeedWidget::UpdateColumnsVisible()
{
    if (!table || !GlobalClient || !GlobalClient->settings)
        return;
    for (int i = 0; i < ColCount; ++i) {
        if (GlobalClient->settings->data.PayloadsTableColumns[i])
            table->showColumn(i);
        else
            table->hideColumn(i);
    }
    fitColumns();
}

void PayloadsFeedWidget::updateEmptyState()
{
    if (!contentStack)
        return;
    const bool has = model && model->rowCount() > 0;
    contentStack->setCurrentIndex(has ? 0 : 1);
    if (emptyLabel)
        emptyLabel->setVisible(!has);
}

void PayloadsFeedWidget::onPageReady(const QJsonObject& response)
{
    applyPage(response);
}

void PayloadsFeedWidget::onPageError(const QString& message)
{
    Q_UNUSED(message);
    m_items.clear();
    if (model)
        model->removeRows(0, model->rowCount());
    updatePaginationChrome(0, 0);
    m_cachePrimed = false;
    updateEmptyState();
}

void PayloadsFeedWidget::AddPayloadItem(const PayloadData& p)
{
    m_items.insert(p.PayloadId, p);
    if (m_cachePrimed) {
        m_offset = 0;
        loadCurrentPage();
    }
}

void PayloadsFeedWidget::UpdatePayloadItem(const PayloadData& p)
{
    m_items.insert(p.PayloadId, p);
    if (m_cachePrimed)
        loadCurrentPage();
}

void PayloadsFeedWidget::UpdatePayloadHidden(const QList<qint64>& ids, bool hidden)
{
    for (qint64 id : ids) {
        if (m_items.contains(id))
            m_items[id].Hidden = hidden;
    }
    if (m_cachePrimed)
        loadCurrentPage();
}

void PayloadsFeedWidget::UpdatePayloadTag(const QList<qint64>& ids, const QString& tag)
{
    for (qint64 id : ids) {
        if (m_items.contains(id))
            m_items[id].Tag = tag;
    }
    if (m_cachePrimed)
        loadCurrentPage();
}

void PayloadsFeedWidget::RemovePayloadItems(const QList<qint64>& ids)
{
    for (qint64 id : ids)
        m_items.remove(id);
    if (m_cachePrimed)
        loadCurrentPage();
}

PayloadData* PayloadsFeedWidget::findById(qint64 id)
{
    auto it = m_items.find(id);
    if (it == m_items.end())
        return nullptr;
    return &it.value();
}

QList<qint64> PayloadsFeedWidget::selectedIds() const
{
    QList<qint64> ids;
    if (!table || !table->selectionModel())
        return ids;
    QSet<qint64> seen;
    const QModelIndexList sel = table->selectionModel()->selectedRows(0);
    for (const QModelIndex& idx : sel) {
        const qint64 id = idx.data(kPayloadIdRole).toLongLong();
        if (id > 0 && !seen.contains(id)) {
            seen.insert(id);
            ids.append(id);
        }
    }
    return ids;
}

void PayloadsFeedWidget::onSearchChanged(const QString& text)
{
    if (!pageHelper)
        return;
    m_offset = 0;
    pageHelper->setFilterText(text);
    pageHelper->setParam(QStringLiteral("show_hidden"), showHidden() ? QStringLiteral("1") : QStringLiteral("0"));
    pageHelper->setParam(QStringLiteral("sort"), m_sortCol);
    pageHelper->setParam(QStringLiteral("order"), m_sortOrder);
}

void PayloadsFeedWidget::onShowHiddenToggled(bool)
{
    m_offset = 0;
    loadCurrentPage();
}

void PayloadsFeedWidget::onGenerateFromToolbar()
{
    actionGenerate();
}

void PayloadsFeedWidget::actionGenerate()
{
    if (!m_adaptixWidget)
        return;
    if (m_adaptixWidget->Listeners.isEmpty()) {
        MessageError(QStringLiteral("No listeners available. Create a listener first."));
        return;
    }
    if (m_adaptixWidget->AgentTypes.isEmpty()) {
        MessageError(QStringLiteral("No agent types registered. Check teamserver extenders."));
        return;
    }

    QStringList agentNames = m_adaptixWidget->AgentTypes.keys();
    agentNames.sort(Qt::CaseInsensitive);

    QStringList agents;
    QMap<QString, AxUI> ax_uis;

    for (const QString& agent : agentNames) {
        const AgentTypeInfo typeInfo = m_adaptixWidget->AgentTypes.value(agent);

        QStringList compatibleTypes;
        QSet<QString> seenTypes;
        for (const auto& listener : m_adaptixWidget->Listeners) {
            if (!typeInfo.listenerTypes.contains(listener.ListenerRegName))
                continue;
            if (seenTypes.contains(listener.ListenerRegName))
                continue;
            seenTypes.insert(listener.ListenerRegName);
            compatibleTypes.append(listener.ListenerRegName);
        }
        if (compatibleTypes.isEmpty())
            continue;

        auto* engine = m_adaptixWidget->ScriptManager->AgentScriptEngine(agent);
        if (!engine) {
            m_adaptixWidget->ScriptManager->consolePrintError(QStringLiteral("Agent %1 is not registered").arg(agent));
            continue;
        }

        QJSValue func = engine->globalObject().property(QStringLiteral("GenerateUI"));
        if (!func.isCallable()) {
            m_adaptixWidget->ScriptManager->consolePrintError(agent + QStringLiteral(" - function GenerateUI is not registered"));
            continue;
        }

        QJSValue jsListeners = engine->newArray(compatibleTypes.size());
        for (int i = 0; i < compatibleTypes.size(); ++i)
            jsListeners.setProperty(i, compatibleTypes[i]);

        QJSValue result = func.call(QJSValueList{jsListeners});
        if (result.isError()) {
            m_adaptixWidget->ScriptManager->consolePrintError(QStringLiteral("%1 GenerateUI: %2").arg(agent, result.toString()));
            continue;
        }
        if (!result.isObject())
            continue;

        QJSValue ui_container = result.property(QStringLiteral("ui_container"));
        QJSValue ui_panel     = result.property(QStringLiteral("ui_panel"));
        QJSValue ui_height    = result.property(QStringLiteral("ui_height"));
        QJSValue ui_width     = result.property(QStringLiteral("ui_width"));

        if (ui_container.isUndefined() || !ui_container.isObject() || ui_panel.isUndefined() || !ui_panel.isQObject())
            continue;

        auto* formElement = dynamic_cast<AxPanelWrapper*>(ui_panel.toQObject());
        auto* container = dynamic_cast<AxContainerWrapper*>(ui_container.toQObject());
        if (!formElement || !container)
            continue;

        const int h = ui_height.isNumber() && ui_height.toInt() > 0 ? ui_height.toInt() : 650;
        const int w = ui_width.isNumber()  && ui_width.toInt()  > 0 ? ui_width.toInt()  : 650;
        agents.append(agent);
        ax_uis[agent] = { container, formElement->widget(), h, w };
    }

    if (agents.isEmpty()) {
        MessageError(QStringLiteral("No agent builders available for current listeners."));
        return;
    }

    QString listenerName;
    QString listenerType;
    {
        const AgentTypeInfo firstInfo = m_adaptixWidget->AgentTypes.value(agents.first());
        for (const auto& listener : m_adaptixWidget->Listeners) {
            if (firstInfo.listenerTypes.contains(listener.ListenerRegName)) {
                listenerName = listener.Name;
                listenerType = listener.ListenerRegName;
                break;
            }
        }
    }
    if (listenerName.isEmpty()) {
        listenerName = m_adaptixWidget->Listeners.first().Name;
        listenerType = m_adaptixWidget->Listeners.first().ListenerRegName;
    }

    auto* dialogAgent = new DialogAgent(m_adaptixWidget, listenerName, listenerType);
    dialogAgent->setAttribute(Qt::WA_DeleteOnClose);
    dialogAgent->SetProfile(*(m_adaptixWidget->GetProfile()));
    dialogAgent->SetAgentTypes(m_adaptixWidget->AgentTypes);
    dialogAgent->SetAvailableListeners(m_adaptixWidget->Listeners);
    dialogAgent->AddExAgents(agents, ax_uis);
    dialogAgent->Start();
}

void PayloadsFeedWidget::actionImport()
{
    if (!m_adaptixWidget || !m_adaptixWidget->GetProfile())
        return;
    QString baseDir = m_adaptixWidget->GetProfile()->GetProjectDir();
    QPointer<PayloadsFeedWidget> safeThis = this;
    NonBlockingDialogs::getOpenFileName(this, "Import payload", baseDir, "All Files (*.*)",
        [safeThis](const QString& filePath) {
            if (!safeThis || filePath.isEmpty())
                return;
            QFile f(filePath);
            if (!f.open(QIODevice::ReadOnly)) {
                MessageError("Failed to open file");
                return;
            }
            QByteArray content = f.readAll();
            f.close();
            QFileInfo fi(filePath);
            AuthProfile* profile = safeThis->m_adaptixWidget->GetProfile();
            HttpReqPayloadImportAsync(fi.completeBaseName(), "imported", fi.suffix(), QString(), QStringList{}, content, QString(), *profile, [](bool success, const QString& message, const QJsonObject&) {
                if (!success)
                    MessageError(message.isEmpty() ? "Import failed" : message);
                else
                    MessageSuccess("Payload imported into store");
            });
        });
}

void PayloadsFeedWidget::actionRemove()
{
    const QList<qint64> ids = selectedIds();
    if (ids.isEmpty())
        return;
    if (!m_adaptixWidget || !m_adaptixWidget->GetProfile())
        return;
    HttpReqPayloadRemoveAsync(ids, false, *m_adaptixWidget->GetProfile(),
        [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Remove failed" : message);
        });
}

void PayloadsFeedWidget::actionDownload()
{
    const QList<qint64> ids = selectedIds();
    if (ids.isEmpty() || !m_adaptixWidget || !m_adaptixWidget->GetProfile())
        return;
    const qint64 id = ids.first();
    PayloadData* p = findById(id);
    QString suggested = p ? p->Filename : QString("payload_%1.bin").arg(id);
    if (suggested.isEmpty())
        suggested = QString("payload_%1.bin").arg(id);
    QString baseDir = m_adaptixWidget->GetProfile()->GetProjectDir();
    QString initial = QDir(baseDir).filePath(suggested);
    QPointer<PayloadsFeedWidget> safeThis = this;

    HttpReqPayloadDownloadAsync(id, *m_adaptixWidget->GetProfile(),
        [safeThis, initial](bool success, const QString& message, const QJsonObject& response) {
            if (!safeThis)
                return;
            if (!success) {
                MessageError(message.isEmpty() ? "Download failed" : message);
                return;
            }
            QString filename = response.value("filename").toString();
            QByteArray content = QByteArray::fromBase64(response.value("content").toString().toUtf8());
            QString path = initial;
            if (!filename.isEmpty())
                path = QDir(QFileInfo(initial).absolutePath()).filePath(filename);
            NonBlockingDialogs::getSaveFileName(safeThis, "Save payload", path, "All Files (*.*)",
                [content](const QString& filePath) {
                    if (filePath.isEmpty())
                        return;
                    QFile file(filePath);
                    if (!file.open(QIODevice::WriteOnly)) {
                        MessageError("Failed to open file for writing");
                        return;
                    }
                    file.write(content);
                    file.close();
                    MessageSuccess(QString("Saved: %1").arg(filePath));
                });
        });
}

void PayloadsFeedWidget::actionCopyHashes()
{
    actionCopyHash(QStringLiteral("all"));
}

void PayloadsFeedWidget::actionCopyHash(const QString& which)
{
    const QList<qint64> ids = selectedIds();
    if (ids.isEmpty())
        return;
    QStringList lines;
    for (qint64 id : ids) {
        PayloadData* p = findById(id);
        if (!p)
            continue;
        if (which == QLatin1String("md5")) {
            if (!p->Md5.isEmpty())
                lines << p->Md5;
        } else if (which == QLatin1String("sha1")) {
            if (!p->Sha1.isEmpty())
                lines << p->Sha1;
        } else if (which == QLatin1String("sha256")) {
            if (!p->Sha256.isEmpty())
                lines << p->Sha256;
        } else {
            lines << QString("#%1 %2").arg(p->PayloadId).arg(p->Name);
            lines << QString("  MD5:    %1").arg(p->Md5);
            lines << QString("  SHA1:   %1").arg(p->Sha1);
            lines << QString("  SHA256: %1").arg(p->Sha256);
        }
    }
    if (lines.isEmpty()) {
        MessageError(QStringLiteral("No hash to copy"));
        return;
    }
    QApplication::clipboard()->setText(lines.join('\n'));
    MessageSuccess(QStringLiteral("Copied to clipboard"));
}

void PayloadsFeedWidget::actionHide()
{
    const QList<qint64> ids = selectedIds();
    if (ids.isEmpty() || !m_adaptixWidget || !m_adaptixWidget->GetProfile())
        return;
    HttpReqPayloadHideAsync(ids, true, *m_adaptixWidget->GetProfile(),
        [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Hide failed" : message);
        });
}

void PayloadsFeedWidget::actionUnhide()
{
    const QList<qint64> ids = selectedIds();
    if (ids.isEmpty() || !m_adaptixWidget || !m_adaptixWidget->GetProfile())
        return;
    HttpReqPayloadHideAsync(ids, false, *m_adaptixWidget->GetProfile(),
        [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Unhide failed" : message);
        });
}

void PayloadsFeedWidget::openPayloadDialog(qint64 id, bool configTab)
{
    if (id <= 0 || !m_adaptixWidget)
        return;
    auto* dlg = new DialogPayload(m_adaptixWidget, id, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->openOnConfigTab(configTab);
    dlg->loadAndShow();
}

void PayloadsFeedWidget::onRowDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    qint64 id = index.sibling(index.row(), ColId).data(kPayloadIdRole).toLongLong();
    if (id <= 0)
        id = index.data(kPayloadIdRole).toLongLong();
    if (id <= 0)
        return;
    openPayloadDialog(id, false);
}

void PayloadsFeedWidget::actionOpenDetails()
{
    const QList<qint64> ids = selectedIds();
    if (ids.isEmpty())
        return;
    openPayloadDialog(ids.first(), false);
}

void PayloadsFeedWidget::actionHardDelete()
{
    const QList<qint64> ids = selectedIds();
    if (ids.isEmpty() || !m_adaptixWidget || !m_adaptixWidget->GetProfile())
        return;
    auto reply = QMessageBox::question(this, QStringLiteral("Delete"), QStringLiteral("Permanently delete %1 payload(s) from disk and database?\nThis cannot be undone.").arg(ids.size()), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;
    HttpReqPayloadRemoveAsync(ids, true, *m_adaptixWidget->GetProfile(),
        [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? QStringLiteral("Delete failed") : message);
        });
}

void PayloadsFeedWidget::actionItemColor()
{
    const QList<qint64> ids = selectedIds();
    if (ids.isEmpty() || !m_adaptixWidget || !m_adaptixWidget->GetProfile())
        return;
    const QColor itemColor = QColorDialog::getColor(Qt::white, this, QStringLiteral("Select items color"));
    if (!itemColor.isValid())
        return;
    HttpReqPayloadSetColorAsync(ids, itemColor.name(), QString(), false, *m_adaptixWidget->GetProfile(),
        [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? QStringLiteral("Response timeout") : message);
        });
}

void PayloadsFeedWidget::actionTextColor()
{
    const QList<qint64> ids = selectedIds();
    if (ids.isEmpty() || !m_adaptixWidget || !m_adaptixWidget->GetProfile())
        return;
    const QColor textColor = QColorDialog::getColor(Qt::white, this, QStringLiteral("Select text color"));
    if (!textColor.isValid())
        return;
    HttpReqPayloadSetColorAsync(ids, QString(), textColor.name(), false, *m_adaptixWidget->GetProfile(),
        [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? QStringLiteral("Response timeout") : message);
        });
}

void PayloadsFeedWidget::actionColorReset()
{
    const QList<qint64> ids = selectedIds();
    if (ids.isEmpty() || !m_adaptixWidget || !m_adaptixWidget->GetProfile())
        return;
    HttpReqPayloadSetColorAsync(ids, QString(), QString(), true, *m_adaptixWidget->GetProfile(),
        [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? QStringLiteral("Response timeout") : message);
        });
}

void PayloadsFeedWidget::actionSetTag()
{
    const QList<qint64> ids = selectedIds();
    if (ids.isEmpty() || !m_adaptixWidget || !m_adaptixWidget->GetProfile())
        return;

    QString tag;
    for (qint64 id : ids) {
        PayloadData* p = findById(id);
        if (!p)
            continue;
        if (tag.isEmpty())
            tag = p->Tag;
    }

    bool inputOk = false;
    const QString newTag = QInputDialog::getText(nullptr, QStringLiteral("Set tags"), QStringLiteral("New tag"), QLineEdit::Normal, tag, &inputOk);
    if (!inputOk)
        return;

    HttpReqPayloadSetTagAsync(ids, newTag, *m_adaptixWidget->GetProfile(),
        [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? QStringLiteral("Response timeout") : message);
        });
}

void PayloadsFeedWidget::handleContextMenu(const QPoint& pos)
{
    if (!table)
        return;

    const QList<qint64> ids = selectedIds();
    bool anyHidden = false;
    bool anyVisible = false;
    for (qint64 id : ids) {
        PayloadData* p = findById(id);
        if (!p)
            continue;
        if (p->Hidden)
            anyHidden = true;
        else
            anyVisible = true;
    }

    oclero::qlementine::Menu menu(this);
    menu.addAction(QIcon(QStringLiteral(":/icons/edit_note")), QStringLiteral("Open details…"), this, &PayloadsFeedWidget::actionOpenDetails);
    menu.addSeparator();
    menu.addAction(QIcon(QStringLiteral(":/icons/downloads")), QStringLiteral("Download"), this, &PayloadsFeedWidget::actionDownload);

    auto* hashMenu = menu.addMenu(QStringLiteral("Copy Hash"));
    hashMenu->addAction(QStringLiteral("MD5"), this, [this]() { actionCopyHash(QStringLiteral("md5")); });
    hashMenu->addAction(QStringLiteral("SHA1"), this, [this]() { actionCopyHash(QStringLiteral("sha1")); });
    hashMenu->addAction(QStringLiteral("SHA256"), this, [this]() { actionCopyHash(QStringLiteral("sha256")); });
    hashMenu->addSeparator();
    hashMenu->addAction(QStringLiteral("All"), this, [this]() { actionCopyHash(QStringLiteral("all")); });

    if (!ids.isEmpty()) {
        menu.addSeparator();
        menu.addAction(QIcon(QStringLiteral(":/icons/tag")), QStringLiteral("Set tag"), this, &PayloadsFeedWidget::actionSetTag);
        auto* appearanceMenu = menu.addMenu(QIcon(QStringLiteral(":/icons/picture")), QStringLiteral("Appearance"));
        appearanceMenu->addAction(QIcon(QStringLiteral(":/icons/color_fill")), QStringLiteral("Set items color"), this, &PayloadsFeedWidget::actionItemColor);
        appearanceMenu->addAction(QIcon(QStringLiteral(":/icons/color_text")), QStringLiteral("Set text color"), this, &PayloadsFeedWidget::actionTextColor);
        appearanceMenu->addAction(QIcon(QStringLiteral(":/icons/color_reset")), QStringLiteral("Reset color"), this, &PayloadsFeedWidget::actionColorReset);
    }

    menu.addSeparator();
    if (anyVisible)
        menu.addAction(QIcon(QStringLiteral(":/icons/visibility_off")), QStringLiteral("Hide"), this, &PayloadsFeedWidget::actionHide);
    if (anyHidden)
        menu.addAction(QIcon(QStringLiteral(":/icons/visibility")), QStringLiteral("Unhide"), this, &PayloadsFeedWidget::actionUnhide);
    menu.addSeparator();
    menu.addAction(QIcon(QStringLiteral(":/icons/delete")), QStringLiteral("Delete"), this, &PayloadsFeedWidget::actionHardDelete);

    if (m_adaptixWidget && m_adaptixWidget->ScriptManager && !ids.isEmpty()) {
        const int before = menu.actions().size();
        const int axCount = m_adaptixWidget->ScriptManager->AddMenuPayloads(&menu, QStringLiteral("PayloadStore"), ids);
        if (axCount > 0 && before > 0) {
            if (QAction* firstAx = menu.actions().value(before))
                menu.insertSeparator(firstAx);
        }
    }

    menu.exec(table->viewport()->mapToGlobal(pos));
}
