#ifndef ADAPTIXCLIENT_LISTENERSFEEDWIDGET_H
#define ADAPTIXCLIENT_LISTENERSFEEDWIDGET_H

#include <Utils/CustomElements/ListFeed.h>
#include <UI/Widgets/AbstractDock.h>

#include <main.h>

class AdaptixWidget;

class ListenersFilterProxy : public QSortFilterProxyModel
{
Q_OBJECT
    QString m_searchText;
    QString m_protocol;

public:
    explicit ListenersFilterProxy(QObject* parent = nullptr);

    void setSearchText(const QString& text);
    void setProtocol(const QString& protocol);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};





class ListenersFeedWidget : public ListFeedWidget
{
Q_OBJECT
    AdaptixWidget* m_adaptixWidget = nullptr;
    KDDockWidgets::QtWidgets::DockWidget* dockWidget = nullptr;

    FeedListModel*        feedBlockModel = nullptr;
    ListenersFilterProxy* filterProxy    = nullptr;

public:
    explicit ListenersFeedWidget(AdaptixWidget* w);
    ~ListenersFeedWidget() override;

    KDDockWidgets::QtWidgets::DockWidget* dock();

    void SetUpdatesEnabled(bool enabled);
    void Clear();
    void AddListenerItem(const ListenerData& newListener);
    void EditListenerItem(const ListenerData& newListener);
    void RemoveListenerItem(const QString& listenerName);

    struct ListenerInfo {
        QString name;
        QString regName;
        QString tags;
        bool    valid = false;
    };
    ListenerInfo currentListenerInfo() const;

protected:
    void onSortingChanged(int index) override;
    void onFilterChanged() override;
    void onGroupModeChanged(int index) override;

public Q_SLOTS:
    void handleFeedMenu(const QPoint& pos);
    void onCreateListener();
    void onEditListener();
    void onRemoveListener();
    void onPauseListener();
    void onResumeListener();
    void onSetTag();
    void onGenerateAgent();
    void onCreateConnector();
};

#endif
