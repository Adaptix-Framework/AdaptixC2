#ifndef ADAPTIXCLIENT_LISTENERSWIDGET_H
#define ADAPTIXCLIENT_LISTENERSWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>

class AdaptixWidget;

class ListenersWidget : public DockTab
{
    AdaptixWidget* adaptixWidget  = nullptr;
    QGridLayout*   mainGridLayout = nullptr;
    QTableWidget*  tableWidget    = nullptr;

    int ColumnName     = 0;
    int ColumnRegName  = 1;
    int ColumnType     = 2;
    int ColumnProtocol = 3;
    int ColumnBindHost = 4;
    int ColumnBindPort = 5;
    int ColumnHosts    = 6;
    int ColumnStatus   = 7;

    void createUI();

public:
    explicit ListenersWidget(AdaptixWidget* w);
    ~ListenersWidget() override;

    void Clear() const;
    void AddListenerItem(const ListenerData &newListener) const;
    void EditListenerItem(const ListenerData &newListener) const;
    void RemoveListenerItem(const QString &listenerName) const;

public Q_SLOTS:
    void handleListenersMenu( const QPoint &pos ) const;
    void onCreateListener() const;
    void onEditListener() const;
    void onRemoveListener() const;
    void onGenerateAgent() const;
};

#endif