#ifndef ADAPTIXCLIENT_TUNNELSFEEDWIDGET_H
#define ADAPTIXCLIENT_TUNNELSFEEDWIDGET_H

#include <Utils/CustomElements/ListFeed.h>
#include <UI/Widgets/AbstractDock.h>

#include <main.h>
#include <QSortFilterProxyModel>

class AdaptixWidget;

class TunnelsFilterProxy : public QSortFilterProxyModel
{
Q_OBJECT
    QString m_searchText;

public:
    explicit TunnelsFilterProxy(QObject* parent = nullptr);

    void setSearchText(const QString& text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};

class TunnelsFeedWidget : public ListFeedWidget
{
Q_OBJECT
    AdaptixWidget* m_adaptixWidget = nullptr;
    KDDockWidgets::QtWidgets::DockWidget* dockWidget = nullptr;

    FeedListModel*      feedBlockModel = nullptr;
    TunnelsFilterProxy* filterProxy    = nullptr;

public:
    explicit TunnelsFeedWidget(AdaptixWidget* w);
    ~TunnelsFeedWidget() override;

    KDDockWidgets::QtWidgets::DockWidget* dock();

    void SetUpdatesEnabled(bool enabled);
    void Clear();
    void AddTunnelItem(const TunnelData& newTunnel);
    void EditTunnelItem(qint64 tunnelId, const QString& info);
    void RemoveTunnelItem(qint64 tunnelId);

    void onFilterChanged() override;

public Q_SLOTS:
    void handleFeedMenu(const QPoint& pos);
    void actionSetInfo();
    void actionStopTunnel();
};

#endif
