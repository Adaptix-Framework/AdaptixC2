#ifndef ADAPTIXCLIENT_LISTENERSWIDGET_H
#define ADAPTIXCLIENT_LISTENERSWIDGET_H

#include <main.h>

class ListenersWidget : public QWidget
{
    QWidget*      mainWidget     = nullptr;
    QGridLayout*  mainGridLayout = nullptr;
    QTableWidget* tableWidget    = nullptr;

    void createUI();

public:
    explicit ListenersWidget( QWidget* w );
    ~ListenersWidget() override;

    void Clear() const;
    void AddListenerItem(const ListenerData &newListener) const;
    void EditListenerItem(const ListenerData &newListener) const;
    void RemoveListenerItem(const QString &listenerName) const;

public slots:
    void handleListenersMenu( const QPoint &pos ) const;
    void createListener() const;
    void editListener() const;
    void removeListener() const;
    void generateAgent() const;
};

#endif //ADAPTIXCLIENT_LISTENERSWIDGET_H