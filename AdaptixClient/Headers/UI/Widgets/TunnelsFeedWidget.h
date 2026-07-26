#ifndef ADAPTIXCLIENT_TUNNELSFEEDWIDGET_H
#define ADAPTIXCLIENT_TUNNELSFEEDWIDGET_H

#include <UI/Widgets/AbstractDock.h>
#include <Utils/CustomElements/ControlCard.h>
#include <main.h>

#include <QWidget>
#include <QVariant>
#include <QLineEdit>

class AdaptixWidget;

class TunnelsFeedWidget : public QWidget
{
Q_OBJECT
    AdaptixWidget* m_adaptixWidget = nullptr;
    KDDockWidgets::QtWidgets::DockWidget* dockWidget = nullptr;

    ControlCardList* m_cardList = nullptr;
    QLineEdit*       m_search   = nullptr;

    QList<TunnelData> m_items;
    qint64 m_selectedId = 0;

    void rebuildVisible();
    ControlCardData toCard(const TunnelData& t) const;
    bool matchesFilter(const TunnelData& t) const;
    TunnelData* findById(qint64 id);
    const TunnelData* findById(qint64 id) const;
    QList<qint64> controllableSelectedIds() const;

public:
    explicit TunnelsFeedWidget(AdaptixWidget* w);
    ~TunnelsFeedWidget() override;

    KDDockWidgets::QtWidgets::DockWidget* dock();

    void SetUpdatesEnabled(bool enabled);
    void Clear();
    void AddTunnelItem(const TunnelData& newTunnel);
    void EditTunnelItem(qint64 tunnelId, const QString& info);
    void SetTunnelActive(qint64 tunnelId, bool active);
    void RemoveTunnelItem(qint64 tunnelId);

public Q_SLOTS:
    void actionSetInfo();
    void actionPauseTunnel();
    void actionResumeTunnel();
    void actionStopTunnel();

private Q_SLOTS:
    void onSearchChanged(const QString& text);
    void onCardPrimary(const QVariant& id);
    void onCardDelete(const QVariant& id);
    void onCardDoubleClick(const QVariant& id);
    void onCardSelected(const QVariant& id);
    void onCardContextMenu(const QVariant& id, const QPoint& globalPos);
};

#endif
