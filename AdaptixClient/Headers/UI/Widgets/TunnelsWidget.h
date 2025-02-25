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
     void EditTunnelItem(QString tunnelId, QString info);
     void RemoveTunnelItem(QString tunnelId);

public slots:
     void handleTunnelsMenu( const QPoint &pos ) const;
     void actionSetInfo();
     void actionStopTunnel();
};

#endif //TUNNELSWIDGET_H
