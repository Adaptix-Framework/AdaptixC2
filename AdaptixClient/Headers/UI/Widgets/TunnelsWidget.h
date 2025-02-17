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
     ~TunnelsWidget();

     void Clear();
     void AddTunnelItem(TunnelData newTunnel);
     void RemoveTunnelItem(QString tunnelId);

public slots:
     void handleTunnelsMenu( const QPoint &pos ) const;
     void stopTunnel();
};

#endif //TUNNELSWIDGET_H
