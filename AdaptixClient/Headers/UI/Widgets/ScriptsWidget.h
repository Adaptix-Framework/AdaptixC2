#ifndef ADAPTIXCLIENT_SCRIPTSDOCKWIDGET_H
#define ADAPTIXCLIENT_SCRIPTSDOCKWIDGET_H

#include <UI/Widgets/AbstractDock.h>
#include <Utils/CustomElements/ListFeed.h>

#include <QSortFilterProxyModel>

class QStackedWidget;
class AdaptixWidget;
class ScriptsFilterProxy;

namespace oclero::qlementine { class SegmentedControl; }

class ScriptsWidget : public DockTab
{
Q_OBJECT
    AdaptixWidget* adaptixWidget;

    ListFeedWidget* m_localFeed  = nullptr;
    FeedListModel*  m_localModel = nullptr;
    ScriptsFilterProxy* m_localFilter = nullptr;

    ListFeedWidget* m_serverFeed  = nullptr;
    FeedListModel*  m_serverModel = nullptr;
    ScriptsFilterProxy* m_serverFilter = nullptr;

    QStackedWidget* m_stack = nullptr;
    int m_currentSegment = 0;

    void setupLocalFeed();
    void setupServerFeed();

public:
    explicit ScriptsWidget(AdaptixWidget* w);
    ~ScriptsWidget() override;

    void refreshLocalScripts();
    void refreshServerScripts();

private Q_SLOTS:
    void onLocalMenu(const QPoint& pos);
    void onServerMenu(const QPoint& pos);
    void onLocalLoad();
    void onLocalReload(const QStringList& paths);
    void onLocalEnable(const QStringList& paths);
    void onLocalDisable(const QStringList& paths);
    void onLocalRemove(const QStringList& paths);
    void onServerEnable(const QStringList& names);
    void onServerDisable(const QStringList& names);
};



class ScriptsFilterProxy : public QSortFilterProxyModel
{
Q_OBJECT
    QString m_searchText;

public:
    explicit ScriptsFilterProxy(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}
    void setSearchText(const QString& text) { m_searchText = text; invalidateFilter(); }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};

#endif
