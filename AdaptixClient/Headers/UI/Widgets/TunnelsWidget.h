#ifndef TUNNELSWIDGET_H
#define TUNNELSWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>

class AdaptixWidget;

class TunnelsWidget : public DockTab
{
     AdaptixWidget* adaptixWidget  = nullptr;
     QGridLayout*   mainGridLayout = nullptr;
     QTableWidget*  tableWidget    = nullptr;

     void createUI();

public:
     explicit TunnelsWidget( AdaptixWidget* w );
     ~TunnelsWidget() override;

     void Clear() const;
     void AddTunnelItem(TunnelData newTunnel) const;
     void EditTunnelItem(const QString &tunnelId, const QString &info) const;
     void RemoveTunnelItem(const QString &tunnelId) const;

public Q_SLOTS:
     void handleTunnelsMenu( const QPoint &pos ) const;
     void actionSetInfo() const;
     void actionStopTunnel() const;
};

#endif
