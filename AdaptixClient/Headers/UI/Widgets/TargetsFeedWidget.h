#ifndef ADAPTIXCLIENT_TARGETSFEEDWIDGET_H
#define ADAPTIXCLIENT_TARGETSFEEDWIDGET_H

#include <Utils/CustomElements/ListFeed.h>
#include <UI/Widgets/AbstractDock.h>
#include <Client/PagedTableHelper.h>

#include <main.h>

class AdaptixWidget;

class TargetsFeedWidget : public ListFeedWidget
{
Q_OBJECT
    AdaptixWidget* m_adaptixWidget = nullptr;
    KDDockWidgets::QtWidgets::DockWidget* dockWidget = nullptr;

    FeedListModel*    feedBlockModel = nullptr;
    PagedTableHelper* pageHelper     = nullptr;
    int               m_offset       = 0;
    QString           m_sortCol      = "Date";
    QString           m_sortOrder    = "desc";
    bool              cachePrimed    = false;
    QHash<qint64, TargetData> m_targetCache;

    void setupPagination();
    void loadCurrentPage();

    struct TargetInfo { qint64 targetId = 0; QString computer; QString domain; QString address; bool valid = false; };
    TargetInfo currentTargetInfo() const;

public:
    explicit TargetsFeedWidget(AdaptixWidget* w);
    ~TargetsFeedWidget() override;

    KDDockWidgets::QtWidgets::DockWidget* dock();

    void SetUpdatesEnabled(bool enabled);
    void Clear();

    void AddTargetsItems(QList<TargetData> targetList);
    void EditTargetsItem(const TargetData& newTarget);
    void RemoveTargetsItem(const QList<qint64>& targetsId);
    void TargetsSetTag(const QList<qint64>& targetIds, const QString& tag);

    void TargetsAdd(QList<TargetData> targetList);

    void UpdateColumnsSize() const {}
    void UpdateColumnsVisible();


protected:
    void onFilterChanged() override;
    void onSortingChanged(int index) override;

public Q_SLOTS:
    void handleFeedMenu(const QPoint& pos);
    void onItemDoubleClicked(const QModelIndex& index);
    void onCreateTarget();
    void onEditTarget();
    void onRemoveTarget();
    void onSetTag();
    void onExportTarget();
    void onCopyToClipboard();

private Q_SLOTS:
    void onPageReady(const QJsonObject& response);
    void onPageError(const QString& message);
};

#endif
