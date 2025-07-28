#ifndef ADAPTIXCLIENT_LISTENERSWIDGET_H
#define ADAPTIXCLIENT_LISTENERSWIDGET_H

#include <main.h>

class AdaptixWidget;

class ListenersWidget : public QWidget
{
    AdaptixWidget* adaptixWidget  = nullptr;
    QGridLayout*   mainGridLayout = nullptr;
    QTableWidget*  tableWidget    = nullptr;

    void createUI();

public:
    explicit ListenersWidget(AdaptixWidget* w);
    ~ListenersWidget() override;

    void Clear() const;
    void AddListenerItem(const ListenerData &newListener) const;
    void EditListenerItem(const ListenerData &newListener) const;
    void RemoveListenerItem(const QString &listenerName) const;

public slots:
    void handleListenersMenu( const QPoint &pos ) const;
    void onCreateListener() const;
    void onEditListener() const;
    void onRemoveListener() const;
    void onGenerateAgent() const;
};

#endif