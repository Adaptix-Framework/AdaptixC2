#ifndef ADAPTIXCLIENT_LISTENERSWIDGET_H
#define ADAPTIXCLIENT_LISTENERSWIDGET_H

#include <main.h>
#include <UI/Dialogs/DialogListener.h>

class ListenersWidget : public QWidget {

    QWidget*       mainWidget     = nullptr;
    QGridLayout*   mainGridLayout = nullptr;
    QTableWidget*  tableWidget    = nullptr;
    QMenu*         menuListeners  = nullptr;

    DialogListener* dialogListener = nullptr;

    void createUI();

public:
    explicit ListenersWidget( QWidget* w );
    ~ListenersWidget();

public slots:
    void handleListenersMenu( const QPoint &pos ) const;
    void CreateListener();

};

#endif //ADAPTIXCLIENT_LISTENERSWIDGET_H