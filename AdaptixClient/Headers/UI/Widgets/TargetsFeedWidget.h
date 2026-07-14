#ifndef ADAPTIXCLIENT_TARGETSFEEDWIDGET_H
#define ADAPTIXCLIENT_TARGETSFEEDWIDGET_H

#include <Utils/CustomElements/ListFeed.h>
#include <UI/Widgets/AbstractDock.h>
#include <UI/Widgets/TargetWidgetIface.h>
#include <Client/PagedTableHelper.h>

#include <main.h>

class AdaptixWidget;

class TargetsFeedWidget : public ListFeedWidget, public TargetWidgetIface
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

    KDDockWidgets::QtWidgets::DockWidget* dock() override;

    void SetUpdatesEnabled(bool enabled) override;
    void Clear() override;

    void AddTargetsItems(QList<TargetData> targetList) override;
    void EditTargetsItem(const TargetData& newTarget) override;
    void RemoveTargetsItem(const QList<qint64>& targetsId) override;
    void TargetsSetTag(const QList<qint64>& targetIds, const QString& tag) override;

    void TargetsAdd(QList<TargetData> targetList) override;

    void UpdateColumnsSize() const override {}
    void UpdateColumnsVisible() override;
    QWidget* asWidget() override { return this; }

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
