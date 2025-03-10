#ifndef TUNNELSWIDGET_H
#define TUNNELSWIDGET_H

#include <main.h>

class TunnelsWidget : public QWidget
{
     QWidget*      mainWidget     = nullptr;
     QGridLayout*  mainGridLayout = nullptr;
     QTableWidget* tableWidget    = nullptr;

     void createUI();

public:
     explicit TunnelsWidget( QWidget* w );
     ~TunnelsWidget() override;

     void Clear() const;
     void AddTunnelItem(TunnelData newTunnel) const;
     void EditTunnelItem(const QString &tunnelId, const QString &info) const;
     void RemoveTunnelItem(const QString &tunnelId) const;

public slots:
     void handleTunnelsMenu( const QPoint &pos ) const;
     void actionSetInfo() const;
     void actionStopTunnel() const;
};

#endif //TUNNELSWIDGET_H
