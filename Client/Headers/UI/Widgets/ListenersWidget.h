#ifndef ADAPTIXCLIENT_LISTENERSWIDGET_H
#define ADAPTIXCLIENT_LISTENERSWIDGET_H

#include <main.h>
#include <UI/Dialogs/DialogListener.h>
#include <UI/Dialogs/DialogAgent.h>

class ListenersWidget : public QWidget
{
    QWidget*       mainWidget     = nullptr;
    QGridLayout*   mainGridLayout = nullptr;
    QTableWidget*  tableWidget    = nullptr;

    void createUI();

public:
    explicit ListenersWidget( QWidget* w );
    ~ListenersWidget();

    void AddListenerItem(ListenerData newListener);
    void EditListenerItem(ListenerData newListener);
    void RemoveListenerItem(QString listenerName);


public slots:
    void handleListenersMenu( const QPoint &pos ) const;
    void createListener();
    void editListener();
    void removeListener();
    void generateAgent();
};

#endif //ADAPTIXCLIENT_LISTENERSWIDGET_H