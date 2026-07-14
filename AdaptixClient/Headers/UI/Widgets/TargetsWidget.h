#ifndef TARGETSWIDGET_H
#define TARGETSWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>
#include <UI/Widgets/TargetWidgetIface.h>
#include <Client/PagedTableHelper.h>
#include <Utils/CustomElements/ClickableLabel.h>
#include <Utils/CustomElements/Delegates.h>
#include <Utils/CustomElements/PageNavBar.h>
#include <Utils/CustomElements/SearchPanel.h>

#include <QSortFilterProxyModel>

class AdaptixWidget;

enum TargetsColumns {
    TRC_Id,
    TRC_Computer,
    TRC_Domain,
    TRC_Address,
    TRC_Tag,
    TRC_Os,
    TRC_Date,
    TRC_Info,
    TRC_Status,
    TRC_ColumnCount
};

static QString sortKeyForTargetSection(int section)
{
    switch (section) {
        case TRC_Id:       return QStringLiteral("TargetId");
        case TRC_Computer: return QStringLiteral("Computer");
        case TRC_Domain:   return QStringLiteral("Domain");
        case TRC_Address:  return QStringLiteral("Address");
        case TRC_Tag:      return QStringLiteral("Tag");
        case TRC_Os:       return QStringLiteral("OsDesk");
        case TRC_Date:     return QStringLiteral("Date");
        case TRC_Info:     return QStringLiteral("Info");
        case TRC_Status:   return QStringLiteral("Alive");
        default:           return {};
    }
}

static void sortTargetsInPlace(QList<TargetData>& list, const QString& col, bool desc)
{
    auto cmp = [&](const TargetData& a, const TargetData& b) -> bool {
        int c = 0;
        if      (col == "TargetId") c = (a.TargetId < b.TargetId) ? -1 : (a.TargetId > b.TargetId ? 1 : 0);
        else if (col == "Computer") c = QString::compare(a.Computer, b.Computer, Qt::CaseInsensitive);
        else if (col == "Domain")   c = QString::compare(a.Domain,   b.Domain,   Qt::CaseInsensitive);
        else if (col == "Address")  c = QString::compare(a.Address,  b.Address,  Qt::CaseInsensitive);
        else if (col == "Tag")      c = QString::compare(a.Tag,      b.Tag,      Qt::CaseInsensitive);
        else if (col == "Os")       c = (a.Os < b.Os) ? -1 : (a.Os > b.Os ? 1 : 0);
        else if (col == "OsDesk")   c = QString::compare(a.OsDesc,   b.OsDesc,   Qt::CaseInsensitive);
        else if (col == "Date")     c = (a.DateTimestamp < b.DateTimestamp) ? -1 : (a.DateTimestamp > b.DateTimestamp ? 1 : 0);
        else if (col == "Info")     c = QString::compare(a.Info,     b.Info,     Qt::CaseInsensitive);
        else if (col == "Alive")    c = (int(a.Alive) < int(b.Alive)) ? -1 : (int(a.Alive) > int(b.Alive) ? 1 : 0);
        else                        c = (a.DateTimestamp < b.DateTimestamp) ? -1 : (a.DateTimestamp > b.DateTimestamp ? 1 : 0);
        return desc ? c > 0 : c < 0;
    };
    std::sort(list.begin(), list.end(), cmp);
}



class TargetsTableModel : public QAbstractTableModel
{
    QVector<TargetData> targets;
    QHash<qint64, int>  idToRow;

    void rebuildIndex() {
        idToRow.clear();
        for (int i = 0; i < targets.size(); ++i)
            idToRow[targets[i].TargetId] = i;
    }

public:
    explicit TargetsTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex&) const override { return targets.size(); }
    int columnCount(const QModelIndex&) const override { return TRC_ColumnCount; }

    bool containsId(qint64 id) const { return idToRow.contains(id); }

    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;

    void add(const TargetData& item);
    void add(const QList<TargetData>& list);
    void update(qint64 targetId, const TargetData& newTarget);
    void remove(const QList<qint64>& targetIds);
    void setTag(const QList<qint64> &targetIds, const QString &tag);
    void clear();
    void reset(const QList<TargetData>& newTargets);
};



class TargetsWidget : public DockTab, public TargetWidgetIface
{
Q_OBJECT
    AdaptixWidget* adaptixWidget  = nullptr;

    QGridLayout* mainGridLayout = nullptr;
    QTableView*  tableView      = nullptr;
    QShortcut*   shortcutSearch = nullptr;

    TargetsTableModel*     targetsModel = nullptr;
    QSortFilterProxyModel* proxyModel   = nullptr;

    PagedTableHelper* pageHelper  = nullptr;
    PageNavBar*       pageNavBar  = nullptr;
    int               m_offset    = 0;
    QString           m_sortCol   = "Date";
    QString           m_sortOrder = "desc";

    bool bufferingEnabled = false;
    bool cachePrimed      = false;

    void createUI();
    void loadCurrentPage();
    void renderFromCache();
    void updateNavBar(int total);
    bool shouldUseCache() const;

public:
    explicit TargetsWidget(AdaptixWidget* w);
    ~TargetsWidget() override;

    KDDockWidgets::QtWidgets::DockWidget* dock() override { return DockTab::dock(); }

    void SetUpdatesEnabled(const bool enabled) override;
    void Clear() override;

    void AddTargetsItems(QList<TargetData> targetList) override;
    void EditTargetsItem(const TargetData &newTarget) override;
    void RemoveTargetsItem(const QList<qint64> &targetsId) override;
    void TargetsSetTag(const QList<qint64> &targetIds, const QString &tag) override;

    void UpdateColumnsSize() const override;
    void UpdateColumnsVisible() override;

    void TargetsAdd(QList<TargetData> targetList) override;

    QWidget* asWidget() override { return this; }

public Q_SLOTS:
    void handleTargetsMenu( const QPoint &pos ) const;
    void onCreateTarget();
    void onEditTarget();
    void onRemoveTarget() const;
    void onSetTag() const;
    void onExportTarget() const;
    void onCopyToClipboard() const;

private Q_SLOTS:
    void onPageReady(const QJsonObject& response);
    void onPageError(const QString& message);
};

#endif //TARGETSWIDGET_H
