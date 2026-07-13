#ifndef ADAPTIXCLIENT_LISTFEED_H
#define ADAPTIXCLIENT_LISTFEED_H

#include <main.h>
#include <UI/Models/GroupingProxyModel.h>

#include <oclero/qlementine/widgets/LineEdit.hpp>
#include <oclero/qlementine/widgets/Switch.hpp>

#include <QListView>
#include <QTreeView>
#include <QAbstractItemView>
#include <QStyledItemDelegate>
#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QReadWriteLock>

struct FeedColors {
    QColor success, error, canceled, running, hosted;
    QColor textPrimary, textSecondary, textDead;
    QColor selectedText, selectedMuted;
    QColor rowBg, rowAltBg, rowHoverBg, rowSelectedBg, rowDeadBg;
    QColor groupBg;
    QColor tagBorder, tagText;
    QColor separatorLine;

    static FeedColors fromTheme();
};

struct FeedPaintContext {
    int maxIdTextWidth = 0;
    bool compact = false;
    int iconSize = 22;
    int tagFontSize = 11;
    int tagBadgeHeight = 20;
};





class PaginationBar : public QWidget
{
Q_OBJECT
    QPushButton* prevBtn       = nullptr;
    QLabel*      infoLabel     = nullptr;
    QPushButton* nextBtn       = nullptr;
    QLabel*      pageSizeLabel = nullptr;
    QSpinBox*    pageSizeSpin  = nullptr;

public:
    explicit PaginationBar(QWidget* parent = nullptr);
    ~PaginationBar() override;

    void setInfo(int from, int to, int total);
    void setPrevEnabled(bool enabled);
    void setNextEnabled(bool enabled);
    void setLoading(bool loading);
    int  pageSize() const;

Q_SIGNALS:
    void prevClicked();
    void nextClicked();
    void pageSizeChanged(int size);
};





class FeedBlock
{
    bool m_stretch = true;
public:
    virtual ~FeedBlock() = default;
    virtual QString name() const = 0;

    virtual int measureWidth(const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont& tinyFont) const = 0;

    virtual void paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont& tinyFont, const QColor& colText, const QColor& colMuted, bool selected, bool dead, const FeedPaintContext& ctx = {}) const = 0;

    enum Policy { LeftFill, RightAlign, Fixed };
    virtual Policy policy() const { return LeftFill; }

    void setStretch(bool on) { m_stretch = on; }
    bool stretch() const { return m_stretch; }

    virtual int hitTest(const QPoint& /*localPos*/, const QRect& /*blockRect*/, const QVariant& /*data*/) const { return -1; }

    virtual int compactDivisions() const { return 1; }

    virtual int measureCompactDivision(int division, const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont& tinyFont) const;

    virtual void paintCompactDivision(QPainter* p, const QRect& subRect, int division, const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont& tinyFont, const QColor& colText, const QColor& colMuted, bool selected, bool dead, const FeedPaintContext& ctx) const;
};



class IconBlock : public FeedBlock {
    QSize m_iconSize{22, 22};
public:
    QString name() const override { return "icon"; }
    Policy policy() const override { return Fixed; }
    int measureWidth(const QVariant& data, const QFont&, const QFont&, const QFont&) const override;
    void paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont&, const QFont&, const QFont&, const QColor&, const QColor&, bool selected, bool, const FeedPaintContext& ctx = {}) const override;
};



class IdBadgeBlock : public FeedBlock {
public:
    QString name() const override { return "id"; }
    Policy policy() const override { return Fixed; }
    int measureWidth(const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont& tinyFont) const override;
    void paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont& tinyFont, const QColor& colText, const QColor& colMuted, bool, bool, const FeedPaintContext& ctx = {}) const override;

    int compactDivisions() const override { return 2; }
    int measureCompactDivision(int division, const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont& tinyFont) const override;
    void paintCompactDivision(QPainter* p, const QRect& subRect, int division, const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont& tinyFont, const QColor& colText, const QColor& colMuted, bool selected, bool dead, const FeedPaintContext& ctx) const override;
};



class MainBlock : public FeedBlock {
public:
    QString name() const override { return "main"; }
    int measureWidth(const QVariant& data, const QFont&, const QFont& smallFont, const QFont&) const override;
    void paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont&, const QFont& smallFont, const QFont&, const QColor& colText, const QColor& colMuted, bool, bool, const FeedPaintContext& = {}) const override;

    int compactDivisions() const override { return 3; }
    int measureCompactDivision(int division, const QVariant& data, const QFont&, const QFont& smallFont, const QFont&) const override;
    void paintCompactDivision(QPainter* p, const QRect& subRect, int division, const QVariant& data, const QFont&, const QFont& smallFont, const QFont&, const QColor& colText, const QColor& colMuted, bool selected, bool dead, const FeedPaintContext& ctx) const override;
};



class TextBlock : public FeedBlock {
public:
    TextBlock() { setStretch(false); }
    QString name() const override { return "text"; }
    Policy policy() const override { return LeftFill; }
    int measureWidth(const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont&) const override;
    void paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont&, const QColor& colText, const QColor& colMuted, bool, bool, const FeedPaintContext& = {}) const override;

    int compactDivisions() const override { return 2; }
    int measureCompactDivision(int division, const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont&) const override;
    void paintCompactDivision(QPainter* p, const QRect& subRect, int division, const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont&, const QColor& colText, const QColor& colMuted, bool, bool, const FeedPaintContext& ctx) const override;
};


class ProgressBlock : public FeedBlock {
public:
    QString name() const override { return "progress"; }
    int measureWidth(const QVariant& data, const QFont&, const QFont& smallFont, const QFont&) const override;
    void paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont&, const QFont& smallFont, const QFont&, const QColor& colText, const QColor& colMuted, bool, bool, const FeedPaintContext& = {}) const override;
};


class TagsBlock : public FeedBlock {
public:
    QString name() const override { return "tags"; }
    Policy policy() const override { return RightAlign; }
    int measureWidth(const QVariant& data, const QFont&, const QFont&, const QFont& tinyFont) const override;
    void paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont&, const QFont&, const QFont& tinyFont, const QColor&, const QColor& colMuted, bool selected, bool, const FeedPaintContext& = {}) const override;
};



class StatusBlock : public FeedBlock {
public:
    QString name() const override { return "right"; }
    Policy policy() const override { return RightAlign; }
    int measureWidth(const QVariant& data, const QFont&, const QFont& smallFont, const QFont&) const override;
    void paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont&, const QFont& smallFont, const QFont&, const QColor& colText, const QColor& colMuted, bool, bool dead, const FeedPaintContext& = {}) const override;

    int compactDivisions() const override { return 2; }
    int measureCompactDivision(int division, const QVariant& data, const QFont&, const QFont& smallFont, const QFont&) const override;
    void paintCompactDivision(QPainter* p, const QRect& subRect, int division, const QVariant& data, const QFont&, const QFont& smallFont, const QFont&, const QColor& colText, const QColor& colMuted, bool selected, bool dead, const FeedPaintContext& ctx) const override;
};



class AttachmentBlock : public FeedBlock {
    static constexpr int BTN_H = 20;
    static constexpr int BTN_PAD = 6;
    static constexpr int BTN_GAP = 4;
public:
    QString name() const override { return "attachment"; }
    Policy policy() const override { return Fixed; }
    int measureWidth(const QVariant& data, const QFont&, const QFont& smallFont, const QFont& tinyFont) const override;
    void paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont&, const QFont& smallFont, const QFont& tinyFont, const QColor& colText, const QColor& colMuted, bool selected, bool dead, const FeedPaintContext& ctx = {}) const override;
    int hitTest(const QPoint& localPos, const QRect& blockRect, const QVariant& data) const override;

    int compactDivisions() const override { return 2; }
    int measureCompactDivision(int division, const QVariant& data, const QFont&, const QFont& smallFont, const QFont& tinyFont) const override;
    void paintCompactDivision(QPainter* p, const QRect& subRect, int division, const QVariant& data, const QFont&, const QFont& smallFont, const QFont& tinyFont, const QColor& colText, const QColor& colMuted, bool selected, bool dead, const FeedPaintContext& ctx) const override;
};



class GroupHeaderBlock : public FeedBlock {
public:
    QString name() const override { return "group"; }
    Policy policy() const override { return LeftFill; }
    int measureWidth(const QVariant&, const QFont&, const QFont&, const QFont&) const override { return 0; }
    void paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont&, const QFont&, const QFont&, const QColor& colText, const QColor& colMuted, bool, bool, const FeedPaintContext& = {}) const override;
};



struct FeedRow
{
    QVector<QVariant> blockData;
    QColor backgroundColor;
    bool isDead = false;
    bool isGroup = false;
    qlonglong entityId = 0;

    QVariant& operator[](int i) { return blockData[i]; }
    const QVariant& operator[](int i) const { return blockData[i]; }
    int size() const { return blockData.size(); }
    void resize(int n) { blockData.resize(n); }
};



class ListFeedModel : public QAbstractListModel
{
Q_OBJECT
public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TypeRole,
        IconRole,
        MainLine1Role,
        MainLine2Role,
        TagsRole,
        StatusRole,
        StatusTextRole,
        LastRole,
        CreatedRole,
        IsDeadRole,
        GroupKeyRole
    };

    explicit ListFeedModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex&) const override { return 0; }
    QVariant data(const QModelIndex&, int) const override { return {}; }
    virtual void sortByRole(int role, Qt::SortOrder order = Qt::AscendingOrder) { Q_UNUSED(role); Q_UNUSED(order); }
};



class FeedListModel : public ListFeedModel
{
Q_OBJECT
    QVector<FeedRow> m_rows;
    QHash<QString, int> m_idToRow;
    int m_groupKeyBlock = -1;
    QString m_groupKeyField;

public:
    explicit FeedListModel(QObject* parent = nullptr) : ListFeedModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void setGroupKeySource(int blockIndex, const QString& fieldKey);
    void addRow(const FeedRow& row);
    void insertRow(int pos, const FeedRow& row);
    void addRows(const QVector<FeedRow>& rows);
    void updateRow(int row, const FeedRow& data);
    void removeRow(int row);
    void clear();

    const FeedRow& rowAt(int i) const;
    int size() const;

    void sortByBlock(int blockIndex, Qt::SortOrder order = Qt::AscendingOrder);
    void sortByField(int blockIndex, const QString& key, Qt::SortOrder order = Qt::AscendingOrder);
    void sortByFieldNumeric(int blockIndex, const QString& key, Qt::SortOrder order = Qt::AscendingOrder);

Q_SIGNALS:
    void rowsContentChanged();
};





class ListFeedDelegate : public QStyledItemDelegate
{
Q_OBJECT

    mutable FeedColors m_cachedColors;
    mutable QStyle*    m_cachedStyle = nullptr;
    mutable QColor     m_cachedPrimaryColor;

    mutable QFont m_monoFont;
    mutable QFont m_smallFont;
    mutable QFont m_tinyFont;
    mutable QFontMetrics* m_fmMono  = nullptr;
    mutable QFontMetrics* m_fmSmall = nullptr;
    mutable QFontMetrics* m_fmTiny  = nullptr;
    mutable bool m_fontsInited      = false;
    void initFonts() const;

    QVector<FeedBlock*> m_blocks;
    FeedListModel* m_feedModel = nullptr;

public:
    explicit ListFeedDelegate(QObject* parent = nullptr);
    ~ListFeedDelegate() override;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

Q_SIGNALS:
    void buttonClicked(int blockIndex, int buttonIndex, const QModelIndex& index);

public:
    void addBlock(FeedBlock* block);
    void setBlocks(const QVector<FeedBlock*>& blocks) { qDeleteAll(m_blocks); m_blocks = blocks; m_widthsDirty = true; }
    int blockCount() const { return m_blocks.size(); }
    FeedBlock* blockAt(int i) const { return m_blocks.value(i, nullptr); }

    void setFeedModel(FeedListModel* model) {
        if (m_feedModel)
            disconnect(m_feedModel, &FeedListModel::rowsContentChanged, this, nullptr);
        m_feedModel = model;
        if (m_feedModel)
            connect(m_feedModel, &FeedListModel::rowsContentChanged, this, [this]{ m_widthsDirty = true; });
        m_widthsDirty = true;
    }
    FeedListModel* feedModel() const { return m_feedModel; }

    void updateMaxWidths(const FeedListModel* model) const;
    void markWidthsDirty() { m_widthsDirty = true; }
    int maxIdWidth() const { return m_cachedMaxIdW; }
    int maxIdTextWidth() const { return m_cachedMaxIdTextW; }
    int maxTextWidth() const { return m_cachedMaxTextW; }
    int maxTagsWidth() const { return m_cachedMaxTagsW; }
    int maxRightWidth() const { return m_cachedMaxRightW; }

    void setCompactMode(bool compact);
    bool isCompactMode() const { return m_compact; }

    void setRowHeights(int normal, int compact);
    void setIconSizes(int normal, int compact);
    void setBlockGaps(int normal, int compact);
    void setBlockGap(int gap);
    void setTagSize(int fontPointSize, int badgeHeight);

protected:
    QFont monoFont() const;
    QFont smallFont() const;
    QFont tinyFont() const;
    QFontMetrics fmMono() const;
    QFontMetrics fmSmall() const;
    QFontMetrics fmTiny() const;

    const FeedColors& feedColors() const;

    int paintIdBadge(QPainter* p, int x, int y, int lh, const QString& idStr, const QString& badgeStr, const QColor& colText, const QColor& colMuted) const;
    int paintTagBadges(QPainter* p, int x, int y, int lh, const QStringList& tags, const QColor& borderColor, const QColor& selectedColor, bool selected) const;
    int paintRightAligned(QPainter* p, int rightX, int y, int lh, const QString& text, const QFont& font, const QColor& color) const;
    int paintSeparator(QPainter* p, int x, int y, int lh) const;
    QColor statusColor(const QString& status, const QColor& fallback) const;

    mutable QVector<int> m_cachedBlockW;
    mutable QVector<QVector<int>> m_cachedSubBlockW;
    mutable int m_cachedMaxIdW = 100;
    mutable int m_cachedMaxIdTextW = 0;
    mutable int m_cachedMaxTagsW = 0;
    mutable int m_cachedMaxRightW = 130;
    mutable int m_cachedMaxTextW = 0;
    mutable int m_cachedMaxStatusW = 0;
    mutable bool m_widthsDirty = true;

    bool m_compact = false;

    int m_normalRowHeight = 54;
    int m_compactRowHeight = 30;
    int m_normalIconSize = 22;
    int m_compactIconSize = 18;
    int m_blockGap = 12;
    int m_normalBlockGap = 12;
    int m_compactBlockGap = 12;
    int m_tagFontSize = 11;
    int m_tagBadgeHeight = 20;
};




class ListFeedWidget : public QWidget
{
Q_OBJECT

public:
    explicit ListFeedWidget(QWidget* parent = nullptr);
    ~ListFeedWidget() override;

    QTreeView* treeView() const { return m_treeView; }
    ListFeedModel* feedModel() const { return m_feedModel; }
    QAbstractItemModel* proxyModel() const { return m_proxyModel; }
    QSortFilterProxyModel* sortProxy() const { return qobject_cast<QSortFilterProxyModel*>(m_proxyModel); }
    GroupingProxyModel* groupingProxy() const { return qobject_cast<GroupingProxyModel*>(m_proxyModel); }

    QModelIndex prepareContextMenuSelection(const QPoint& viewportPos) const;

    static QModelIndex prepareContextMenuSelection(QAbstractItemView* view, const QPoint& viewportPos);

    void setDelegate(ListFeedDelegate* delegate);
    void setModel(ListFeedModel* model);
    void setFilterModel(QSortFilterProxyModel* filter);

    void enableGrouping(bool enable, AdaptixWidget* aw = nullptr);
    void setGroupingField(int field);
    bool isGroupingEnabled() const { return m_groupingEnabled; }

    void enableSearch(bool enable);
    void enableGroupCombo(bool enable);
    void enableActiveFilter(bool enable, const QString& label = QStringLiteral("active only"));
    void enablePagination(bool enable);
    void enableAutoCheck(bool enable);
    void enableFilterCombo(bool enable, const QString& placeholder = "All");
    void enableSortingCombo(bool enable, const QStringList& items = {});
    void addToolbarWidgetBefore(QWidget* widget);
    void addToolbarWidgetAfter(QWidget* widget);
    void finalizeSearchWidget();

    void setCompactMode(bool compact);
    bool isCompactMode() const;

    void enableCompactMode(bool enable) { setCompactMode(enable); }

    void enableCompactSwitch(bool enable);
    oclero::qlementine::Switch* compactSwitch() const { return m_compactSwitch; }

    void setRowHeights(int normal, int compact);
    void setIconSizes(int normal, int compact);
    void setBlockGaps(int normal, int compact);
    void setBlockGap(int gap);
    void setTagSize(int fontPointSize, int badgeHeight);

    oclero::qlementine::LineEdit* searchInput() const { return m_searchInput; }
    QAction* autoAction() const { return m_autoAction; }
    QComboBox* groupCombo() const { return m_groupCombo; }
    QCheckBox* activeFilter() const { return m_activeFilter; }
    QComboBox* filterCombo() const { return m_filterCombo; }
    QComboBox* sortingCombo() const { return m_sortingCombo; }
    QAction* sortOrderAction() const { return m_sortOrderAction; }
    bool isSortAscending() const { return m_sortAscending; }
    PaginationBar* paginationBar() const { return m_paginationBar; }

    virtual void onFilterChanged();
    virtual void onGroupModeChanged(int index);
    virtual void onSortingChanged(int index);

protected:
    QGridLayout* m_mainLayout = nullptr;
    QTreeView* m_treeView = nullptr;
    ListFeedModel* m_feedModel = nullptr;
    QSortFilterProxyModel* m_filterModel = nullptr;
    QAbstractItemModel* m_proxyModel = nullptr;

    bool m_groupingEnabled = false;
    int m_groupingField = 0;
    AdaptixWidget* m_groupingAdaptixWidget = nullptr;

    oclero::qlementine::LineEdit* m_searchInput = nullptr;

    QWidget*       m_toolbarWidget   = nullptr;
    QHBoxLayout*   m_toolbarLayout   = nullptr;
    QWidget*       m_searchWidget    = nullptr;
    QAction*       m_autoAction      = nullptr;
    QComboBox*     m_groupCombo      = nullptr;
    QCheckBox*     m_activeFilter    = nullptr;
    QComboBox*     m_filterCombo     = nullptr;
    QComboBox*     m_sortingCombo    = nullptr;
    QAction*       m_sortOrderAction = nullptr;
    bool           m_sortAscending   = true;
    PaginationBar* m_paginationBar   = nullptr;

    bool m_compactMode = false;
    oclero::qlementine::Switch* m_compactSwitch = nullptr;

    int m_storedNormalRowH = 54;
    int m_storedCompactRowH = 30;
    int m_storedNormalIcon = 22;
    int m_storedCompactIcon = 18;
    int m_storedNormalGap = 12;
    int m_storedCompactGap = 12;
    int m_storedTagFont = 11;
    int m_storedTagBadgeH = 20;

    void setupConnections();
public:
    void rebuildModelChain();
    void rebuildSearchWidget();
};

#endif
